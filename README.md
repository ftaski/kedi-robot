# Kedi Robotu

Makeblock mBot'u (mCore kartı) kedi oyuncağına çeviren Arduino programı ve Bluetooth kontrol sayfası.

## Robot ne yapar

- **Dolaşır:** Av gibi hareket eder; ani atılır, sinsice ilerler, döner, donup kalır.
- **Kediyle oynar:** Önünde hareket eden bir şey görünce onu kışkırtır. Kedi çok yaklaşırsa kaçar, sonra durup kovalanmayı bekler.
- **Duvardan döner:** Önündeki şey kıpırdamıyorsa duvar sayar ve başka yöne gider.
- Kartın üstündeki düğmeyle başlar ve durur. Ayarlanan süre dolunca kendiliğinden durur.

Ultrasonik sensör yalnız mesafe ölçer; kediyi tanıyamaz, hareket eden şeyi kedi sayar.

## Donanım

- mBot, mCore kartı (ATmega328P)
- Ultrasonik sensör **port 4**'te
- Makeblock Bluetooth modülü (uygulama için)
- Motorlar zayıf pille kartı yeniden başlatabildiği için yumuşak kalkış ve hız sınırı var. Taze pil kullan.

## Yükleme

```
arduino-cli compile --fqbn arduino:avr:uno --upload -p COM3 kedi-robot
```

## Kontrol sayfası

**https://ftaski.github.io/kedi-robot/**

iPhone ve iPad'de Safari Bluetooth'u desteklemez; sayfayı **Bluefy** tarayıcısıyla aç. Bilgisayarda Chrome ya da Edge çalışır.

Sayfada başlat ve durdur düğmeleri, joystick ile elle sürme, canlı mesafe ve robotun modu, hız, kaçma mesafesi ve oyun süresi ayarları var. Joystick bırakılınca ya da bağlantı koparsa robot 1 saniye içinde durur.

## Seri protokol (115200, satır başına bir komut)

| Yön | Komut | Anlamı |
|---|---|---|
| uygulama → robot | `S` | oyunu başlat |
| | `X` | durdur |
| | `M,<sol>,<sağ>` | elle sürme, -255..255; 1 saniyeden sık tekrarlanmalı |
| | `C,<hız>,<kaçma cm>,<dakika>` | ayarlar, robot kapanınca sıfırlanır |
| robot → uygulama | `D,<cm>,<mod>,<hız>,<kaçma cm>,<dakika>` | 300 ms'de bir; mod: I bekliyor, W dolaşıyor, P oynuyor, M elle |
