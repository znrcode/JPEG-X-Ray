#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Veri tipleri için takma isimler
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

// JPEG İşaretçileri (Markers)
#define M_BASLANGIC 0xD8 // SOI
#define M_BITIS     0xD9 // EOI
#define M_UYG0      0xE0 // APP0 JFIF verisi çıkarır
#define M_UYG1      0xE1 // APP1 EXIF/XMP verisi çıkaır
#define M_UYG2      0xE2 // APP2 ICC verisi çıkarır
#define M_UYG13     0xED // APP13 Photoshop verisi çıkarır
#define M_KARE0     0xC0 // SOF0
#define M_KARE2     0xC2 // SOF2
#define M_K_TABLOSU 0xDB // DQT Quantization Tablosu çıkarır jpeg kaiitesi tahmini yapar
#define M_YORUM     0xFE // COM verisi çıkarır

// Görüntü bilgilerini tutan değişken yapsısı
typedef struct {
    int genislik, yukseklik;
    int x_yogunluk, y_yogunluk;
    char yogunluk_birimi[32];

    char marka[64];
    char model[64];
    char cekim_tarihi[64];
    char degisme_tarihi[64];
    char yazilim[128];
    char telif_hakki[128];

    int iso_degeri;
    double pozlama, f_degeri, odak_uzakligi;

    int gps_var_mi;
    double enlem, boylam;

    int icc_var_mi;
    int yorum_var_mi;
    char yorum[256];

    int xmp_var_mi;
    char xmp_etiketi[32];

    int photoshop_var_mi;
    int onizleme_var_mi;
    int jpeg_kalitesi;
    double x_cozunurluk, y_cozunurluk;
    char cozunurluk_birimi[32];

} BILGI;

// 16-bit tam sayı okuma fonksiyonu
uint16_t oku16(uint8_t *p, int be) {
    return be ? (p[0] << 8 | p[1]) : (p[1] << 8 | p[0]);
}

// 32-bit tam sayı okuma fonksiyonu
uint32_t oku32(uint8_t *p, int be) {
    return be ? (p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]) :
                (p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0]);
}

// Rasyonel sayı okur Exif'teki pay/payda yapısını oluşturur
double rasyonel_oku(uint8_t *b, uint32_t ofset, int be) {
    uint32_t pay = oku32(b + ofset, be);
    uint32_t payda = oku32(b + ofset + 4, be);
    if (!payda) return 0;
    return (double)pay / payda;
}

// JFIF verilerini ayrıştırır yoğunluk çıkarır
void jfif_ayristir(uint8_t *b, BILGI *bilgi) {
    bilgi->x_yogunluk = (b[8] << 8 | b[9]);
    bilgi->y_yogunluk = (b[10] << 8 | b[11]);

    int birim = b[7];
    if (birim == 1) strcpy(bilgi->yogunluk_birimi, "dots per inch");
    else if (birim == 2) strcpy(bilgi->yogunluk_birimi, "dots per cm");
    else strcpy(bilgi->yogunluk_birimi, "unknown");
}

// Kare başlangıcı (SOF) verilerini ayrıştırır
void sof_ayristir(uint8_t *b, BILGI *bilgi) {
    bilgi->yukseklik = (b[1] << 8 | b[2]);
    bilgi->genislik = (b[3] << 8 | b[4]);
}

// GPS verilerini ayrıştırır ve çözümler, kordinatlarını hespalar
void gps_ayristir(uint8_t *t, uint32_t ofset, int be, BILGI *bilgi) {
    uint8_t *ifd = t + ofset;
    int sayac = oku16(ifd, be);
    ifd += 2;

    char enlem_ref = 0, boylam_ref = 0;
    double enlem_dizi[3] = {0}, boylam_dizi[3] = {0};

    for (int k = 0; k < sayac; k++) {
        uint16_t etiket = oku16(ifd, be);
        uint16_t tip = oku16(ifd + 2, be);
        uint32_t deger = oku32(ifd + 8, be);

        if (tip == 2 && deger < 0x1000000) {
            char *p = (char*)(t + deger);
            if (etiket == 1 && p && (p[0] == 'N' || p[0] == 'S')) enlem_ref = p[0];
            if (etiket == 3 && p && (p[0] == 'E' || p[0] == 'W')) boylam_ref = p[0];
        }
        else if (etiket == 2 && tip == 5) {
            for (int j = 0; j < 3; j++) enlem_dizi[j] = rasyonel_oku(t, deger + j * 8, be);
        }
        else if (etiket == 4 && tip == 5) {
            for (int j = 0; j < 3; j++) boylam_dizi[j] = rasyonel_oku(t, deger + j * 8, be);
        }
        ifd += 12;
    }

    bilgi->enlem = enlem_dizi[0] + enlem_dizi[1] / 60.0 + enlem_dizi[2] / 3600.0;
    bilgi->boylam = boylam_dizi[0] + boylam_dizi[1] / 60.0 + boylam_dizi[2] / 3600.0;

    if (enlem_ref == 'S') bilgi->enlem = -bilgi->enlem;
    if (boylam_ref == 'W') bilgi->boylam = -bilgi->boylam;

    if (enlem_dizi[0] != 0 || boylam_dizi[0] != 0) bilgi->gps_var_mi = 1;
}

// Alt-IFD (Exif detayları) ayrıştıran kod blogu
void alt_ifd_ayristir(uint8_t *t, uint32_t ofset, int be, BILGI *bilgi) {
    uint8_t *ifd = t + ofset;
    int sayac = oku16(ifd, be);
    ifd += 2;

    for (int k = 0; k < sayac; k++) {
        uint16_t etiket = oku16(ifd, be);
        uint32_t deger = oku32(ifd + 8, be);

        switch (etiket) {
            case 0x9003: strcpy(bilgi->cekim_tarihi, (char*)(t + deger)); break;
            case 0x8827: bilgi->iso_degeri = oku16(t + deger, be); break;
            case 0x829A: bilgi->pozlama = rasyonel_oku(t, deger, be); break;
            case 0x829D: bilgi->f_degeri = rasyonel_oku(t, deger, be); break;
            case 0x920A: bilgi->odak_uzakligi = rasyonel_oku(t, deger, be); break;
        }
        ifd += 12;
    }
}

// EXIF ana verilerini ayrıştır
void exif_ayristir(uint8_t *tampon, BILGI *bilgi) {
    if (memcmp(tampon, "Exif\0\0", 6)) return;

    uint8_t *t = tampon + 6;
    int be = (t[0] == 'M'); // endians kontrolü yapar

    uint32_t ofset = oku32(t + 4, be);
    uint8_t *ifd = t + ofset;
    int sayac = oku16(ifd, be);
    ifd += 2;

    for (int k = 0; k < sayac; k++) {
        uint16_t etiket = oku16(ifd, be);
        uint16_t tip = oku16(ifd + 2, be);
        uint32_t deger = oku32(ifd + 8, be);

        uint8_t *veri = (tip == 2 || tip == 5) ? t + deger : ifd + 8;

        switch (etiket) {
            case 0x011A: // X yatay çözünürlük verisi çıkarır
                if (tip == 5) bilgi->x_cozunurluk = rasyonel_oku(t, deger, be);
                break;
            case 0x011B: // Y  dikey  çözünürlük verisi çıkarır
                if (tip == 5) bilgi->y_cozunurluk = rasyonel_oku(t, deger, be);
                break;
            case 0x0128: // çözünürlük  bölümünün birimi
                if (tip == 3) {
                    uint16_t birim = oku16(veri, be);
                    if (birim == 2) strcpy(bilgi->cozunurluk_birimi, "DPI");
                    else if (birim == 3) strcpy(bilgi->cozunurluk_birimi, "DPCM");
                    else strcpy(bilgi->cozunurluk_birimi, "Unknown");
                }
                break;
            case 0x010F: strcpy(bilgi->marka, (char*)(t + deger)); break;
            case 0x0110: strcpy(bilgi->model, (char*)(t + deger)); break;
            case 0x0132: strcpy(bilgi->degisme_tarihi, (char*)(t + deger)); break;
            case 0x0131: strcpy(bilgi->yazilim, (char*)(t + deger)); break;
            case 0x8298: strcpy(bilgi->telif_hakki, (char*)(t + deger)); break;
            case 0x8769: alt_ifd_ayristir(t, deger, be, bilgi); break;
            case 0x8825: gps_ayristir(t, deger, be, bilgi); break;
            case 0x0201: bilgi->onizleme_var_mi = 1; break;
        }
        ifd += 12;
    }
}

// XMP verilerini kolayca tarar
void xmp_ayristir(uint8_t *b, BILGI *bilgi) {
    char *x = strstr((char*)b, "xmpmeta");
    if (!x) return;

    bilgi->xmp_var_mi = 1;
    if (strstr(x, "Label=\"Red\""))
        strcpy(bilgi->xmp_etiketi, "Red");
}

// ICC profilini kontrol eeder
void icc_ayristir(uint8_t *b, int uzunluk, BILGI *bilgi) {
    if (uzunluk > 14 && !memcmp(b, "ICC_PROFILE", 11)) bilgi->icc_var_mi = 1;
}

// JPEG yorumlarını yani comment varsa ayrıştırır
void yorum_ayristir(uint8_t *b, int uzunluk, BILGI *bilgi) {
    int s = uzunluk - 2; 
    if (s > 255) s = 255;
    memcpy(bilgi->yorum, b, s);
    bilgi->yorum[s] = '\0';
    bilgi->yorum_var_mi = 1;
}

// APP13 Photoshop verilerini kontrol eder
void uyg13_ayristir(uint8_t *b, int uzunluk, BILGI *bilgi) {
    if (uzunluk > 14 && !memcmp(b, "Photoshop", 9)) bilgi->photoshop_var_mi = 1;
}

// Kuantizasyon tablosundan yaklaşık JPEG kalitesini hesaplayan dqt ayrıştırma fonksiyonudur.
void dqt_ayristir(uint8_t *b, int uzunluk, BILGI *bilgi) {
    if (uzunluk < 65) return;
    int toplam = 0;
    for (int k = 1; k <= 64; k++) toplam += b[k];
    int q = 100 - (toplam / 64);
    if (q < 1) q = 1; if (q > 100) q = 100;
    bilgi->jpeg_kalitesi = q;
}

// Raporu ekrana yazdıran fonksiyondur
void raporu_yazdir(BILGI *bilgi, char *dosya_adi) {
    printf("File: %s\n", dosya_adi);

    printf("\n[Basic JPEG Info]\n");
    printf("Width              : %s\n", bilgi->genislik ? "OK" : "Not Available");
    if (bilgi->genislik) printf("Width Value        : %d\n", bilgi->genislik);
    printf("Height             : %s\n", bilgi->yukseklik ? "OK" : "Not Available");
    if (bilgi->yukseklik) printf("Height Value       : %d\n", bilgi->yukseklik);

    if (bilgi->x_yogunluk) {
        printf("Resolution         : %d x %d (%s)\n",
               bilgi->x_yogunluk, bilgi->y_yogunluk, bilgi->yogunluk_birimi);
    } else {
        printf("JFIF Density       : Not Available\n");
    }

    printf("\n[EXIF]\n");
    printf("Camera Make        : %s\n", strlen(bilgi->marka) ? bilgi->marka : "Not Available");
    printf("Camera Model       : %s\n", strlen(bilgi->model) ? bilgi->model : "Not Available");
    printf("Date Taken         : %s\n", strlen(bilgi->cekim_tarihi) ? bilgi->cekim_tarihi : "Not Available");
    printf("Modify Date        : %s\n", strlen(bilgi->degisme_tarihi) ? bilgi->degisme_tarihi : "Not Available");
    printf("Software           : %s\n", strlen(bilgi->yazilim) ? bilgi->yazilim : "Not Available");
    printf("Copyright          : %s\n", strlen(bilgi->telif_hakki) ? bilgi->telif_hakki : "Not Available");

    printf("\n[EXIF Resolution]\n");
    if (bilgi->x_cozunurluk && bilgi->y_cozunurluk) {
        printf("X Resolution       : %.2f\n", bilgi->x_cozunurluk);
        printf("Y Resolution       : %.2f\n", bilgi->y_cozunurluk);
        printf("Resolution Unit    : %s\n", strlen(bilgi->cozunurluk_birimi) ? bilgi->cozunurluk_birimi : "Not Available");
    } else {
        printf("Not Available\n");
    }

    printf("ISO                : %s\n", bilgi->iso_degeri ? "OK" : "Not Available");
    if (bilgi->iso_degeri) printf("ISO Value          : %d\n", bilgi->iso_degeri);

    printf("Exposure Time      : %s\n", bilgi->pozlama ? "OK" : "Not Available");
    if (bilgi->pozlama) printf("Exposure Value     : %.4f sec\n", bilgi->pozlama);

    printf("F Number           : %s\n", bilgi->f_degeri ? "OK" : "Not Available");
    if (bilgi->f_degeri) printf("F Number Value     : %.2f\n", bilgi->f_degeri);

    printf("Focal Length       : %s\n", bilgi->odak_uzakligi ? "OK" : "Not Available");
    if (bilgi->odak_uzakligi) printf("Focal Length Value : %.2f mm\n", bilgi->odak_uzakligi);

    if (bilgi->gps_var_mi) {
        printf("\n[GPS]\n");
        printf("Latitude           : %.6f\n", bilgi->enlem);
        printf("Longitude          : %.6f\n", bilgi->boylam);
        printf("https://www.google.com/maps/search/?api=1&query=%.6f,%.6f", bilgi->enlem, bilgi->boylam);
    } else {
        printf("\n[GPS]\nNot Available\n");
    }

    if (bilgi->xmp_var_mi) {
        printf("\n[XMP]\n");
        printf("Label              : %s\n", strlen(bilgi->xmp_etiketi) ? bilgi->xmp_etiketi : "Not Available");
    } else printf("\n[XMP]\nNot Available\n");

    if (bilgi->yorum_var_mi) {
        printf("\n[Comment]\nText               : %s\n", bilgi->yorum);
    } else printf("\n[Comment]\nNot Available\n");

    if (bilgi->icc_var_mi) printf("\n[ICC]\nICC Profile        : Present\n");
    else printf("\n[ICC]\nNot Available\n");

    if (bilgi->onizleme_var_mi) printf("\n[Thumbnail]\nPresent\n");
    else printf("\n[Thumbnail]\nNot Available\n");

    if (bilgi->photoshop_var_mi) printf("\n[APP13]\nPhotoshop Metadata Present\n");

    if (bilgi->jpeg_kalitesi) printf("\n[DQT]\nApprox JPEG Quality : %d\n", bilgi->jpeg_kalitesi);
}

// Ana Fonksiyon
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("jpeg-xray file.jpg\n");
        return 1;
    }

    FILE *dosya = fopen(argv[1], "rb");
    if (!dosya) return 1;

    // JPEG SOI kontrolü
    if (fgetc(dosya) != 0xFF || fgetc(dosya) != M_BASLANGIC) {
        printf("Not JPEG\n");
        fclose(dosya);
        return 1;
    }

    BILGI bilgi = {0};

    while (1) {
        int c = fgetc(dosya);
        if (c == EOF) break;
        if (c != 0xFF) continue;

        uint8_t isaretci = fgetc(dosya);
        if (isaretci == M_BITIS) break;

        uint16_t uzunluk = (fgetc(dosya) << 8) | fgetc(dosya);
        if (uzunluk < 2) break;

        uint8_t *tampon = malloc(uzunluk - 2);
        fread(tampon, 1, uzunluk - 2, dosya);

        // İşaretçiye göre ayrıştırma
        if (isaretci == M_UYG0 && !memcmp(tampon, "JFIF", 4)) jfif_ayristir(tampon, &bilgi);
        else if (isaretci == M_UYG1) { exif_ayristir(tampon, &bilgi); xmp_ayristir(tampon, &bilgi); }
        else if (isaretci == M_UYG2) icc_ayristir(tampon, uzunluk - 2, &bilgi);
        else if (isaretci == M_UYG13) uyg13_ayristir(tampon, uzunluk - 2, &bilgi);
        else if (isaretci == M_KARE0 || isaretci == M_KARE2) sof_ayristir(tampon, &bilgi);
        else if (isaretci == M_K_TABLOSU) dqt_ayristir(tampon, uzunluk - 2, &bilgi);
        else if (isaretci == M_YORUM) yorum_ayristir(tampon, uzunluk, &bilgi);

        free(tampon);
    }
    
    fclose(dosya);
    raporu_yazdir(&bilgi, argv[1]);
    return 0;
}