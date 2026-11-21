# JPEG X-Ray

C diliyle geliştirilen JPEG dosyası inceleme aracı. JPEG dosyasının yapısını okuyarak görüntü bilgilerini ve dosyada bulunan bazı metadata alanlarını terminalde gösterir.

## Özellikler

- Görüntü genişliği ve yüksekliği
- JFIF çözünürlük bilgileri
- EXIF: kamera markası/modeli, çekim tarihi, ISO, pozlama, diyafram ve odak uzaklığı
- GPS koordinatları
- XMP, ICC ve Photoshop metadata kontrolü
- JPEG yorumları
- DQT tablosundan yaklaşık kalite değeri

## Derleme

GCC ile:

```bash
gcc -std=c11 -Wall -Wextra -O2 jpeg-xray.c -o jpeg-xray
```

Windows için:

```bash
gcc -std=c11 -Wall -Wextra -O2 jpeg-xray.c -o jpeg-xray.exe
```

## Kullanım

```bash
./jpeg-xray image.jpg
```

Windows:

```powershell
.\jpeg-xray.exe image.jpg
```

## Not

Araç temel JPEG ve EXIF alanlarını okumak için geliştirilmiştir. Tüm JPEG/EXIF formatlarını desteklemez. DQT üzerinden gösterilen kalite değeri yaklaşık bir tahmindir.
