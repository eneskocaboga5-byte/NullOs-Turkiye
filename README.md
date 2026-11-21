# NullOS — RP2040 İçin Modüler Bare-Metal Mikro-Çekirdek

**NullOS**, Raspberry Pi Pico (RP2040) üzerinde hiçbir harici SDK (`pico-sdk`, `FreeRTOS` vb.) kullanmadan sıfırdan geliştirilmiş, modüler mimariye sahip bare-metal bir işletim sistemi çekirdeğidir.

USB CDC sanal seri port (`/dev/ttyACM0`) üzerinden çalışan yerleşik bir **POSIX benzeri kabuğa (`nsh`)**, sanal dosya sistemine (`RAMFS`), çift çekirdek (`SMP Core 1`) desteğine ve **`nul.conf`** tabanlı modül yapılandırma sistemine sahiptir.

## Temel Özellikler

- **Modüler Yapılandırma (`nul.conf`):** İstediğiniz sürücü ve alt sistemleri (`=y` / `=n`) açıp kapatabilirsiniz. Derleme sırasında `tools/gen_kconfig.py` otomatik olarak `autoconf.h` ve `kconfig.mk` üreterek yalnızca aktif modülleri derler.
- **Evrensel Cihaz Sürücü Modeli (`device.h`):** Donanım sürücüleri (`GPIO_0`, `ADC_0`, `PWM_0`, `I2C_0`) çekirdeğe kayıt olur ve çalışma zamanında `device list` komutuyla durumları denetlenebilir.
- **Çift Çekirdek (`SMP`) Desteği:** RP2040'ın ikinci ARM Cortex-M0+ çekirdeği (`Core 1`) SIO FIFO mailbox üzerinden uyandırılır ve kabuğu dondurmadan arka plan görevlerini yürütür.
- **Sanal Dosya Sistemi (`RAMFS`):** `/`, `/proc`, `/dev`, `/etc`, `/home` dizin yapısını ve dinamik çekirdek dosyalarını (`/proc/cpuinfo` vb.) destekler.
- **Düğmesiz Yazılım Güncelleme (`bootsel`):** RP2040 Bootrom API'si sayesinde fiziksel `BOOTSEL` düğmesine basmadan tek komutla yazılım yükleme (`RPI-RP2`) moduna geçer.

## Proje Mimarisi

```text
├── nul.conf                  # Modül aç/kapa yapılandırma dosyası
├── Makefile                  # build/ dizini altına izole derleme sistemi
├── tools/
│   ├── gen_kconfig.py        # nul.conf -> autoconf.h & kconfig.mk üretici
│   └── elf2uf2.py            # RP2040 UF2 imaj dönüştürücü
├── drivers/                  # Düşük seviye USB CDC seri port sürücüsü
└── src/
    ├── main.c                # Çekirdek başlatma giriş noktası
    ├── kernel/               # Zamanlayıcı, Bootrom API, dmesg günlüğü ve cihaz kaydı
    ├── drivers/              # GPIO, ADC (sıcaklık), PWM ve I2C sürücüleri
    └── subsys/               # RAMFS dosya sistemi, Core 1 SMP ve POSIX Shell
```

## Kurulum ve Derleme

### Gereksinimler
- `arm-none-eabi-gcc` ve `arm-none-eabi-binutils`
- `python3`

### Derleme ve Karta Yükleme
```bash
git clone https://github.com
cd NullOS
make
```
Derleme tamamlandığında tüm çıktılar `build/` klasöründe oluşturulur (`build/nullos.uf2`).

1. Kartı `BOOTSEL` modunda bilgisayara bağlayın (veya çalışan sistemde `bootsel` komutunu kullanın).
2. Oluşan `.uf2` dosyasını karta kopyalayın:
   ```bash
   cp build/nullos.uf2 /run/media/\$USER/RPI-RP2/ && sync
   ```
3. Seri konsola bağlanın:
   ```bash
   screen /dev/ttyACM0 115200
   ```

## `nul.conf` Yapılandırması

Kök dizindeki `nul.conf` dosyasını düzenleyerek aktif modülleri özelleştirebilirsiniz:

```ini
CONFIG_GPIO=y
CONFIG_ADC=y
CONFIG_PWM=y
CONFIG_I2C=y
CONFIG_SMP=y
CONFIG_RAMFS=y
CONFIG_BOOTSEL_CMD=y
```

## Yerleşik Kabuk (`nsh`) Komutları

| Kategori | Komut | Açıklama |
| :--- | :--- | :--- |
| **Sistem** | `help`, `uname`, `uptime`, `dmesg` | Çekirdek sürümü, çalışma süresi ve açılış günlüğü |
| **Cihazlar** | `device list` | Aktif donanım sürücülerini ve `READY` durumlarını listeler |
| **Donanım** | `gpio <pin> <on\|off\|toggle\|read>` | 30 GPIO pinini canlı kontrol eder |
| **Sensör & Sinyal** | `adc temp`, `pwm <pin> <0-100>`, `i2c scan` | Çip sıcaklığını ölçer, PWM üretir ve I2C veri yolunu tarar |
| **Çoklu Çekirdek** | `core1 <idle\|heartbeat\|stress\|stat>` | 2. çekirdeğin çalışma modunu ve döngü sayacını yönetir |
| **Dosya Sistemi** | `ls`, `cd`, `pwd`, `mkdir`, `write`, `cat`, `rm` | Sanal `RAMFS` üzerinde dosya ve dizin işlemleri yapar |
| **Bellek & Güç** | `peek <hex>`, `poke <hex> <val>`, `reboot`, `bootsel` | Doğrudan register erişimi ve yeniden başlatma |

## Lisans ve Atıf

Bu proje, Douglas H. Summerville tarafından geliştirilen [Bare-Metal-Raspberry-PI-Pico](https://github.com) donanım başlatma ve USB CDC altyapısı üzerine inşa edilmiştir ve **MIT Lisansı** altında dağıtılmaktadır.
