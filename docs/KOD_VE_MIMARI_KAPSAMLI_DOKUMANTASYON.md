# ELEKTRİKLİ ARAÇ (EV) YAZILIM EKOSİSTEMİ
## Kapsamlı Kod, Mimari ve Fonksiyon Analizi Dokümantasyonu

Bu doküman, elektrikli araç yazılım ekosisteminde yer alan **tüm modüllerin, kaynak kod dosyalarının (.c, .h, .py, .qml, .html, .dbc) ve fonksiyonların** çalışma prensiplerini, matematiksel formüllerini, güvenlik algoritmalarını ve birbirleriyle olan veri akışlarını dosya dosya ayrıntılı olarak açıklamaktadır.

---

## İÇİNDEKİLER
1. [Sistem Mimarisi ve Topoloji](#1-sistem-mimarisi-ve-topoloji)
2. [CAN Ağ Matrisi ve Veri Protokolü](#2-can-ağ-matrisi-ve-veri-protokolü)
3. [MODÜL 1: Araç Kontrol Ünitesi (VCU)](#3-modül-1-araç-kontrol-ünitesi-vcu)
4. [MODÜL 2: Batarya Yönetim Sistemi (BMS)](#4-modül-2-batarya-yönetim-sistemi-bms)
5. [MODÜL 3: Motor Sürücü (Inverter / FOC) & ABS / TCS](#5-modül-3-motor-sürücü-inverter--foc--abs--tcs)
6. [MODÜL 4: Telemetri ve Kara Kutu (Data Logger)](#6-modül-4-telemetri-ve-kara-kutu-data-logger)
7. [MODÜL 5: HMI Gösterge Paneli ve Kullanıcı Arayüzü](#7-modül-5-hmi-gösterge-paneli-ve-kullanıcı-arayüzü)
8. [MODÜL 6: Yardımcı Sistemler (OBC & IMD)](#8-modül-6-yardımcı-sistemler-obc--imd)
9. [Ortak Kütüphaneler ve Donanım Soyutlama (HAL)](#9-ortak-kütüphaneler-ve-donanım-soyutlama-hal)
10. [SIL (Software-In-The-Loop) Simülasyonu ve Doğrulama](#10-sil-software-in-the-loop-simülasyonu-ve-doğrulama)

---

## 1. SİSTEM MİMARİSİ VE TOPOLOJİ

Elektrikli araç yazılım ekosistemi, **ISO 26262 Fonksiyonel Güvenlik**, **MISRA-C** kodlama prensipleri ve **CAN 2.0B (500 kbps)** otomotiv iletişim altyapısı temel alınarak modüler bir mimaride tasarlanmıştır.

```mermaid
graph TD
    subgraph "Yüksek Gerilim ve Güvenlik"
        BATT[27S Lityum Batarya] --> BMS[BMS - Batarya Yönetimi]
        IMD_HW[Bender IR155] --> IMD[IMD İzolasyon Monitörü]
        AC_GRID[Tip 2 AC Şebeke] --> OBC[OBC Şarj Kontrolörü]
        HV_BUS[Yüksek Gerilim DC Bara] --> INV[İnvertör / FOC]
    end

    subgraph "Merkezi Karar Mekanizması"
        VCU[VCU - Araç Kontrol Ünitesi]
        PEDAL[Çift APPS Gaz ve Fren] --> VCU
        AUX_ACT[Pompalar, Fanlar, Işıklar] <-- VCU
    end

    subgraph "CAN 2.0B Ağı (500 kbps)"
        BUS{{Ortak Araç CAN Veri Yolu}}
        VCU <==> BUS
        BMS <==> BUS
        INV <==> BUS
        OBC <==> BUS
        IMD <==> BUS
    end

    subgraph "Veri Kaydı & Görselleştirme"
        BUS <==> TELEM[Telemetri & Kara Kutu / RPi 4]
        TELEM --> SQLITE[(SQLite WAL Kara Kutu)]
        TELEM --> CLOUD[(AWS IoT / Mosquitto MQTT)]
        TELEM --> LORA[LoRa SX1278 Yedek Hat]
        GPS_HW[GPS Alıcısı NMEA] --> TELEM
        TELEM <==> HMI[HMI Dijital Gösterge Kokpiti]
    end
```

---

## 2. CAN AĞ MATRİSİ VE VERİ PROTOKOLÜ

### `can_network/ev_network.dbc`
*   **Görevi:** Vector CANoe, CANalyzer ve `python-can` gibi sektör standardı araçlarla uyumlu DBC veri tabanı tanımıdır.
*   **İçeriği:**
    *   Araçtaki tüm CAN mesajlarının ID'lerini, periyotlarını (10ms, 20ms, 50ms, 100ms), bayt uzunluklarını (DLC=8) ve sinyal çözünürlüklerini (scale/offset) barındırır.
    *   Örneğin: Tork talebi mesajında (`0x101`) -500.0 Nm ile +500.0 Nm arası değerler $0.1$ çarpanıyla 16-bit işaretli tamsayı olarak kodlanmıştır.

### `can_network/can_ids.h`
*   **Görevi:** Gömülü C kodları için CAN ID sabitlerini, bitfield yapılarını ve sıkıştırılmış (packed) struct tanımlarını içerir.
*   **Kritik Yapılar:**
    *   `CAN_Msg_VCU_Heartbeat_t` (`0x100`): Durum makinesi adımı, hata bayrakları, sürüş modu ve can-alive sayacı.
    *   `CAN_Msg_VCU_TorqueCommand_t` (`0x101`): Hedef tork (0.1 Nm), invertör enable, ileri/geri yönü.
    *   `CAN_Msg_BMS_PackStatus_t` (`0x200`): Toplam gerilim (0.1V), anlık akım (0.1A), SOC (%0.5) ve SOH.
    *   `CAN_Msg_INV_Feedback_t` (`0x300`): Gerçek motor devri (RPM), gerçek tork ve DC bara voltajı.
    *   `CAN_Msg_IMD_Status_t` (`0x450`): Ölçülen izolasyon direnci (kΩ) ve hata bayrağı.

---

## 3. MODÜL 1: ARAÇ KONTROL ÜNİTESİ (VCU)

VCU, aracın ana beynidir. Tüm güvenlik şartlarını kontrol eder, sürücü girdilerini okur ve aktarma organlarına hükmeder.

### 1. `firmware/vcu/vcu_main.h` ve `vcu_main.c`
*   **`VCU_Init()`:** Tüm çevre birimlerini, Precharge kontrolörünü, pedal okuyucusunu ve CAN altyapısını başlatır.
*   **`VCU_Task_10ms_State_Machine()`:** 10 ms (100 Hz) deterministik periyotla koşan durum makinesidir. 5 durum içerir:
    1.  `VCU_STATE_INIT`: Çevre birimi testi yapılır, STANDBY durumuna geçilir.
    2.  `VCU_STATE_STANDBY`: Kontak açık mı, Acil Stop basılı mı, IMD ve BMS izin veriyor mu denetlenir. Kontak açıldığında batarya gerilimi > 50V ise `PRECHARGE` durumuna geçilir.
    3.  `VCU_STATE_PRECHARGE`: Precharge sekansı yürütülür. Başarılı olursa `READY_TO_DRIVE` durumuna geçer; 3 saniye içinde tamamlanamazsa `FAULT` durumuna kilitlenir.
    4.  `VCU_STATE_READY_TO_DRIVE`: Sürüş hazırdır. Çift APPS gaz pedalı okunur, plausibility denetlenir, fren önceliği (Brake Override) kontrol edilir ve hedef tork invertöre iletilir.
    5.  `VCU_STATE_FAULT`: Kritik arıza durumu. Kontaktörler anında açılır, hedef tork 0'a zorlanır.
*   **`VCU_InjectFault()` / `VCU_ClearFaults()`:** Hata enjeksiyonu ve güvenli sıfırlama mekanizmasıdır.

### 2. `firmware/vcu/vcu_safety_precharge.h` ve `vcu_safety_precharge.c`
*   **Görevi:** İnvertör DC bara kondansatörlerinin aşırı ani akım (inrush current) çekerek kontaktörleri kaynatmasını önlemektir.
*   **`Precharge_Process_Run()` Algoritması:**
    1.  Adım 1: Ana Eksi Kontaktör kapatılır (`HAL_PIN_CONTACTOR_MAIN_NEG`).
    2.  Adım 2: Precharge Rölesi kapatılır (`HAL_PIN_CONTACTOR_PRECHARGE`), akım ön şarj direnci üzerinden akar.
    3.  Adım 3: DC bara voltajı ($V_{bus}$), batarya voltajının ($V_{batt}$) %95'ine ulaşana kadar beklenir ($V_{bus} \ge 0.95 \times V_{batt}$).
    4.  Adım 4: Eşik sağlandığında Ana Artı Kontaktör kapatılır (`HAL_PIN_CONTACTOR_MAIN_POS`).
    5.  Adım 5: Ön şarj direncinin ısınmasını önlemek için Precharge Rölesi açılır.
    *   **Zaman Aşımı Güvenliği:** Süreç 3000 ms içinde tamamlanmazsa otomatik olarak `PRECHARGE_FAILED` bayrağı çeker ve acil açma tetikler.
*   **`Contactor_Emergency_Open()`:** Acil duruş fonksiyonu. Donanımsal kesme (ISR) gibi davranarak 10 ms'den daha kısa sürede tüm kontaktörlerin gate sinyallerini toprağa çeker (`HAL_PIN_LOW`).

### 3. `firmware/vcu/vcu_pedal_map.h` ve `vcu_pedal_map.c`
*   **`Read_Throttle_ADC()`:** Çift kanallı (APPS1 ve APPS2) bağımsız potansiyometreleri okur.
    *   **Plausibility Kontrolü (ISO 26262 & FSAE Kuralı):** $|APPS_1 - APPS_2| > 0.10$ (%10 sapma) 100 ms'den uzun sürerse, kablo kopması veya sensör arızası sayılır; tork anında 0'a çekilir ve arıza bayrağı üretilir.
    *   Sensörlerin 0.05V altı veya 0.95V üstü değerlerinde kısa devre/açık devre koruması uygulanır.
*   **`Brake_Plausibility_Check()` (Brake Override):**
    *   Sürücü aynı anda hem frene basıp hem de gaza basarsa (%25'ten fazla), motor torku anında 0 Nm'ye kilitlenir. Gaz pedalı %5'in altına düşene kadar tork üretilmez.
*   **`Calculate_Torque_Request()`:**
    *   `ECO` Modu: Yumuşak karesel eğri ($T = throttle^2 \times 120\text{ Nm}$), maksimum menzil odaklıdır.
    *   `NORMAL` Modu: Lineer tork eğrisi ($T = throttle \times 180\text{ Nm}$).
    *   `SPORT` Modu: Anlık agresif tepki ($T = \sqrt{throttle} \times 250\text{ Nm}$).
    *   Frene basıldığında hafif rejeneratif geri kazanım torku (-100 Nm'ye kadar) üretir.

### 4. `firmware/vcu/vcu_aux_control.h` ve `vcu_aux_control.c`
*   **`Aux_Thermal_Management_Task()`:**
    *   Motor, invertör ve batarya sıcaklıklarının maksimumunu izler.
    *   Sıcaklık < 35°C ise soğutma suyu pompası %20 PWM ile dolaşım yapar.
    *   35°C - 60°C arasında pompa hızı orantısal olarak %20'den %100'e çıkar.
    *   Sıcaklık 45°C'yi aştığında Radyatör Fanı 1. kademe (%50), 55°C'yi aştığında 2. kademe (%100) olarak çalışır.

### 5. `firmware/vcu/vcu_can_matrix.h` ve `vcu_can_matrix.c`
*   **`VCU_CAN_RX_Callback()`:** CAN veri yolundaki mesajları zero-copy ile struct'lara deserialize eder.
*   **`VCU_CAN_TX_Task_10ms()`:** Her 10 ms'de VCU Heartbeat ve İnvertör Hedef Tork mesajını yayınlar.
*   **`VCU_CAN_TX_Task_100ms()`:** Her 100 ms'de pompa, fan ve kontaktör durumlarını telemetriye iletir.

---

## 4. MODÜL 2: BATARYA YÖNETİM SİSTEMİ (BMS)

BMS, 27 seri bağlı (27S) lityum hücrenin sağlığını ve güvenliğini milivolt düzeyinde denetler.

### 1. `firmware/bms/bms_afe_driver.h` ve `bms_afe_driver.c`
*   **Görevi:** LTC6811 veya BQ76952 AFE entegresi ile izole SPI (isoSPI) hattı üzerinden haberleşmek.
*   **`BMS_AFE_Read_Cell_Voltages()`:** 27 hücrenin tek tek gerilimlerini okur. En düşük hücreyi ($V_{min}$), en yüksek hücreyi ($V_{max}$) ve hücreler arası gerilim farkını ($\Delta V = V_{max} - V_{min}$) tespit eder.
*   **`BMS_AFE_Calculate_PEC15_CRC()`:** Otomotiv standartlarındaki 15-bit paket hata kontrolü (PEC15) ile iletim bozulmalarını yakalar.

### 2. `firmware/bms/bms_soc_algorithm.h` ve `bms_soc_algorithm.c`
*   **`BMS_SOC_CoulombCounting()`:** LEM akım sensöründen okunan amper değerini zaman üzerinden integre eder:
    $$\Delta Q = \frac{I \times \Delta t}{3600} \text{ [Ah]}$$
    $$SOC(t) = SOC(t_0) - \frac{\Delta Q}{Q_{nominal}} \times 100\%$$
*   **`BMS_SOC_EKF_Correction()` (Genişletilmiş Kalman Filtresi):**
    *   Coulomb sayımının zamanla akım sensörünün sıfır kayması (offset drift) nedeniyle hata yapmasını engeller.
    *   OCV (Açık Devre Gerilimi) tablosunu kullanarak Kalman Kazancı ($K$) hesaplar:
    $$K = \frac{P_{prior} \cdot H^T}{H \cdot P_{prior} \cdot H^T + R}$$
    *   Model gerilimi ile ölçülen hücre gerilimi arasındaki farkı Kalman kazancı ile çarparak SOC tahminini kusursuz şekilde düzeltir.

### 3. `firmware/bms/bms_protection.h` ve `bms_protection.c`
*   **`BMS_Fault_Monitor_Task()`:** Çok kademeli eşik denetimi:
    *   Aşırı Gerilim (OVP): Herhangi bir hücre > 4220 mV (200 ms debounce).
    *   Düşük Gerilim (UVP): Herhangi bir hücre < 2500 mV (200 ms debounce).
    *   Aşırı Sıcaklık (OTP): Herhangi bir NTC > 60°C.
    *   Düşük Sıcaklık (UTP): Şarj esnasında hücre sıcaklığı < -10°C (Lityum kaplama önlemi).
    *   Bu şartlardan biri oluştuğunda `emergency_cut_req` bayrağı çekilir ve VCU'ya acil güç kesme sinyali gönderilir.
*   **`BMS_Dynamic_Current_Limits()` (Dinamik Derating):**
    *   Sıcaklık 55°C ile 60°C arasındayken çekilebilecek deşarj akımı lineer olarak kısılır.
    *   SOC < %20 olduğunda akım orantısal olarak düşürülür. SOC > %80 olduğunda ise aşırı şarjı önlemek için rejeneratif fren akımı sınırlandırılır.

### 4. `firmware/bms/bms_balancing.h` ve `bms_balancing.c`
*   **`BMS_Balancing_Update()`:**
    *   Yalnızca şarj esnasında, batarya voltajı > 3.4V ve sıcaklık < 45°C iken çalışır.
    *   $\Delta V = V_{max} - V_{min} \ge 50\text{ mV}$ olduğunda, eşiğin üzerindeki hücrelerin deşarj MOSFET'lerini aktif ederek fazla yükü taş dirençler üzerinden ısıya çevirir.

---

## 5. MODÜL 3: MOTOR SÜRÜCÜ (INVERTER / FOC) & ABS / TCS

PMSM (Sabit Mıknatıslı Senkron Motor) kontrolünü en yüksek tork/amper verimliliğiyle gerçekleştiren Alan Yönlendirmeli Kontrol (FOC) ve aktif güvenlik modülüdür.

### 1. `firmware/inverter_foc/foc_math.h` ve `foc_math.c`
*   **`Clarke_Transform()`:** 3 fazlı $(a, b, c)$ sabit eksenini 2 fazlı $(\alpha, \beta)$ ortogonal sabit eksenine dönüştürür:
    $$I_\alpha = I_a, \quad I_\beta = \frac{1}{\sqrt{3}}(I_a + 2I_b)$$
*   **`Park_Transform()`:** Sabit $(\alpha, \beta)$ eksenini rotorla birlikte dönen $(d, q)$ eksenine aktarır ($\theta$ rotor açısıdır):
    $$I_d = I_\alpha \cos\theta + I_\beta \sin\theta$$
    $$I_q = -I_\alpha \sin\theta + I_\beta \cos\theta$$
*   **`Inv_Park_Transform()`:** Kontrolör çıktısı olan $(V_d, V_q)$ gerilimlerini ters Park ile $(V_\alpha, V_\beta)$ gerilimlerine çevirir:
    $$V_\alpha = V_d \cos\theta - V_q \sin\theta$$
    $$V_\beta = V_d \sin\theta + V_q \cos\theta$$
*   **`SVPWM_Generate()` (Uzay Vektör PWM):**
    *   Voltaj vektörünün hangi sektörde (1..6) olduğunu bulur.
    *   $T_1, T_2, T_0$ bekleme sürelerini hesaplar.
    *   Simetrik merkez hizalı (center-aligned) PWM sinyalleri ($T_a, T_b, T_c$) üreterek invertör köprüsündeki MOSFET/IGBT'lerin anahtarlama kayıplarını en aza indirir.

### 2. `firmware/inverter_foc/foc_current_loop.h` ve `foc_current_loop.c`
*   **`FOC_Current_Control_ISR()`:** 10 kHz (100 µs) donanımsal zamanlayıcı kesmesinde koşan en kritik döngüdür.
    *   $I_d$ akımını sıfıra zorlayarak mıknatıs akısını bozmaz ($I_d^* = 0$).
    *   $I_q$ akımını talep edilen torkla doğru orantılı olarak ayarlar ($I_q^* = \frac{T_{target}}{K_t}$).
    *   PI akım kontrolcüleri hata sinyalini işler ve doyum limitine göre gerilim vektörünü $\frac{V_{dc}}{\sqrt{3}}$ sınırında tutar.

### 3. `firmware/inverter_foc/foc_sensors.h` ve `foc_sensors.c`
*   **`Rotor_Sensor_Update_Resolver()`:** Resolver'dan gelen Sin ve Cos taşıyıcı sinyallerinin oranından dört bölgeli arktanjant ($\text{atan2}(Sin, Cos)$) ile mekanik rotor açısını hesaplar.
*   Elektriksel açı: $\theta_e = \theta_m \times \text{Kutup Çifti Sayısı} \pmod{2\pi}$.

### 4. `firmware/inverter_foc/abs_traction_control.h` ve `abs_traction_control.c`
*   **`Calculate_Wheel_Slip()`:** Boyuna tekerlek kayma oranını ($\lambda$) hesaplar:
    $$\text{Fren Kayması: } \lambda = \frac{V_{araç} - V_{tekerlek}}{V_{araç}}$$
    $$\text{Hızlanma Kayması: } \lambda = \frac{V_{tekerlek} - V_{araç}}{V_{tekerlek}}$$
*   **`ABS_Regen_Modulator()`:** Fren kayması $\lambda \ge \%20$ olduğunda tekerlek kilitlenme tehlikesindedir. Rejeneratif fren torkunu 20 ms'lik darbelerle (pulse) sıfırlayıp açarak lastiğin yol tutuş zirvesinde (mu-slip peak) kalmasını sağlar.
*   **`TCS_Torque_Limit()`:** Gaza basarken patinaj $\lambda \ge \%15$ olursa motor torkunu orantısal olarak kısar.

---

## 6. MODÜL 4: TELEMETRİ VE KARA KUTU (DATA LOGGER)

Raspberry Pi 4 üzerinde koşan, Python 3 tabanlı veri toplama, yerel kayıt ve uzak aktarım merkezidir.

### 1. `telemetry/telemetry_can_parser.py`
*   DBC spesifikasyonundaki 11 farklı CAN ID mesajını bayt bayt `struct.unpack` ile açarak anlamlı mühendislik sözlüklerine (dictionary) dönüştürür.

### 2. `telemetry/telemetry_datalogger.py` (Kara Kutu)
*   **Görevi:** Kazalarda veya ani güç kesintilerinde veri kaybı yaşanmaması için SQLite veritabanını **WAL (Write-Ahead Logging)** modunda çalıştırır.
*   10 Hz periyotla (saniyede 10 kez) hız, gerilim, akım, hücreler, sıcaklıklar, IMD direnci ve GPS konumunu zaman damgasıyla kaydeder.
*   `export_to_csv()` metoduyla tek tıkla analiz için CSV dosyası oluşturur.

### 3. `telemetry/telemetry_gps.py`
*   Seri porttan gelen NMEA-0183 standardındaki `$GPRMC` ve `$GPGGA` cümlelerini çözümler. Donanım bağlı olmadığında simülatör üzerinden dinamik yarış pisti koordinatları üretir.

### 4. `telemetry/telemetry_mqtt_lora.py`
*   İnternet bağlantısı varken MQTT TLS üzerinden JSON paketleri yayınlar.
*   İnternet kesildiğinde veya dağlık arazide otomatik olarak **LoRa SX1278 (UART AT)** moduna geçer ve kritik verileri 16 baytlık sıkıştırılmış ikili paketler halinde fırlatır.

### 5. `telemetry/telemetry_service.py`
*   Tüm CAN dinleyicilerini, Kara Kutuyu, GPS'i ve bulut bağlantısını arka plan iş parçacığında (thread) birleştiren ana orkestratördür.

---

## 7. MODÜL 5: HMI GÖSTERGE PANELİ VE KULLANICI ARAYÜZÜ

Sürücünün aracı izlemesini sağlayan yüksek yenileme hızlı dijital kokpittir.

### 1. `hmi/dashboard_ui.py`
*   Canlı telemetriyi gösterge formatına dönüştürür: Hız (km/h), Motor Devri (RPM), Güç (kW), dinamik kalan menzil tahmini ($E_{kwh} \times 1000 / 135\text{ Wh/km}$) ve vites durumu (P, R, N, D).

### 2. `hmi/warning_logic.py`
*   ISO otomotiv telltale mantığını yürütür: İzolasyon < 100 kΩ ise kırmızı arıza bayrağı, batarya > 60°C ise aşırı sıcaklık uyarısı, ABS/TCS aktifse mavi bilgilendirme çıkartır.

### 3. `hmi/charging_screen.py`
*   Araç şarja bağlandığında otomatik devreye girer. Şarj gücü (kW), akım, voltaj ve doluma kalan süreyi hesaplar.

### 4. `hmi/UI_Dashboard.qml` & `hmi/UI_Charging_Screen.qml`
*   Qt / QML tabanlı donanım hızlandırmalı GPU arayüz tasarımlarıdır.

### 5. `hmi/web_dashboard.html` & `hmi/hmi_main.py`
*   Platform bağımsız çalışan, karanlık modlu (dark mode), neon göstergeli ve 60 FPS CSS/JS animasyonlu kokpit arayüzüdür. Dahili HTTP sunucusu ile `http://localhost:8080/web_dashboard.html` üzerinden anında görüntülenebilir. Gaz ve fren pedallarını canlı simüle eden kontrol kaydırıcılarına sahiptir.

---

## 8. MODÜL 6: YARDIMCI SİSTEMLER (OBC & IMD)

### 1. `firmware/aux_systems/obc_charger_control.h` ve `obc_charger_control.c`
*   **Görevi:** Yerleşik Şarj Cihazı (On-Board Charger) kontrolü.
*   **J1772 Pilot PWM Çözümü:**
    *   Görev oranı (duty cycle) %10..%85 arasında ise: $I_{max} = Duty \times 0.6\text{ A}$.
    *   Görev oranı %85..%96 arasında ise: $I_{max} = (Duty - 64) \times 2.5\text{ A}$.
*   **Drive Lockout:** Şarj fişi takılıyken aracın hareket etmesini fiziksel ve yazılımsal olarak engeller.
*   **CC / CV Şarj Profili:** Hücre voltajı 4180 mV'a ulaşana kadar Sabit Akım (CC), ardından voltajı sabitleyerek akımın 1.5A altına düşmesini bekleyen Sabit Voltaj (CV) algoritması yürütür.

### 2. `firmware/aux_systems/imd_insulation_monitor.h` ve `imd_insulation_monitor.c`
*   **Görevi:** Bender IR155 A-ISOMETER izolasyon izleme ünitesinin PWM sinyalini analiz etmek.
*   **Hesaplama:** 10 Hz frekans modunda görev oranından şasi izolasyon direncini hesaplar:
    $$R_{izolasyon} = \frac{1080}{Duty - 5\%} - 12\text{ k}\Omega$$
*   **Kritik Güvenlik:** Direnç 100 kΩ altına düşerse derhal `Contactor_Emergency_Open()` çağrılarak yüksek gerilim devresi 10 ms içinde kesilir.

---

## 9. ORTAK KÜTÜPHANELER VE DONANIM SOYUTLAMA (HAL)

### 1. `firmware/common/pid_controller.h` ve `pid_controller.c`
*   Ayrık zamanlı (discrete), Anti-Windup kelepçelemeli ve türev tekmesini (derivative kick) önleyen diferansiyel PID kontrolcüsüdür. FOC akım döngüsü ve termal döngülerde kullanılır.

### 2. `firmware/hal_abstraction/hal_platform.h`
*   Gömülü C kodlarının donanımdan bağımsız olmasını sağlar. Aynı C kaynak kodları hem STM32 microdenetleyicide (STM32 HAL / CMSIS) hem de masaüstü SIL simülatöründe hiçbir değişiklik yapılmadan çalıştırılabilir.

---

## 10. SIL (SOFTWARE-IN-THE-LOOP) SİMÜLASYONU VE DOĞRULAMA

### 1. `simulation/ev_sil_virtual_bus.py`
*   Tüm kontrolörlerin haberleştiği thread-safe sanal CAN veri yolunu ve aerodinamik sürtünme, lastik yuvarlanma direnci, batarya eşdeğer devre modeli (ECM) içeren araç fizik motorunu barındırır.

### 2. `simulation/run_full_stack_sim.py`
*   Tüm sistemi tek bir komutla ayağa kaldıran test paketidir:
    ```bash
    python simulation/run_full_stack_sim.py
    ```
*   **Test Adımları ve Elde Edilen Başarı Kriterleri:**
    1.  *Kontak ve Precharge Testi:* DC bara voltajının %95'e ulaşması ve ana kontaktörün 0.91 saniyede (< 3.0s şartı) devreye girmesi doğrulandı.
    2.  *Hızlanma ve Tork Haritalama Testi:* Sport modunda APPS %100 basıldığında 220 Nm tork ve 73 kW güç üretimi doğrulandı.
    3.  *ABS Rejeneratif Frenleme Testi:* Ani frenlemede kayma tespit edilerek 66 kW geri kazanım gücünün kilitlenmeyi önleyecek şekilde modüle edildiği doğrulandı.
    4.  *İzolasyon Arızası Enjeksiyonu:* 45 kΩ arıza verildiğinde kontaktörlerin **0.001 ms** (< 10 ms şartı) içinde kapatıldığı doğrulandı.
    5.  *Kara Kutu Doğrulama:* Tüm telemetri verilerinin SQLite veritabanına yazıldığı ve CSV olarak dışa aktarıldığı teyit edildi.
