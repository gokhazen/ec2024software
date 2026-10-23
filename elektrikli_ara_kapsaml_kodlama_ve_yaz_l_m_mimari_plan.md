# ELEKTRİKLİ ARAÇ (EV) KAPSAMLI KODLAMA VE YAZILIM MİMARİ PLANI

Bu doküman, elektrikli bir aracın A'dan Z'ye tüm yazılım ekosistemini oluşturmak için bir kodlama rehberidir. Her modül, yazılacak kaynak kod dosyalarına (.c, .h, .cpp, .py) ayrılmış ve içermesi gereken fonksiyonlar, algoritmalar ve donanım hedefleri detaylandırılmıştır.

---

## MODÜL 1: ARAÇ KONTROL ÜNİTESİ (VCU / AKS)
**Hedef Donanım:** STM32F4xx veya STM32G4xx Serisi (Mikrodenetleyici)
**Dil:** C / C++ (FreeRTOS tabanlı)
**Görev:** Aracın beyni; pedal okuma, güvenlik, kontaktör kontrolü ve diğer birimlerin orkestrasyonu.

### 1. `vcu_main.c` / `vcu_main.h`
*   **İçerik:** FreeRTOS task oluşturma (TaskInit), sistem saat ayarları (SystemClock_Config), Watchdog Timer (WDT) başlatma.
*   **Kritik Fonksiyonlar:**
    *   `VCU_Init()`: Tüm pinlerin ve çevre birimlerinin (CAN, ADC, GPIO) başlatılması.
    *   `Task_State_Machine()`: 10ms periyotla koşan, aracın durumunu (INIT, STANDBY, PRECHARGE, READY_TO_DRIVE, FAULT) yöneten ana durum makinesi.

### 2. `vcu_safety_precharge.c` / `vcu_safety_precharge.h`
*   **İçerik:** Yüksek gerilim kontaktörlerinin (Ana Eksi, Pre-Charge, Ana Artı) sıralı ve güvenli şekilde kapatılıp açılması.
*   **Kritik Fonksiyonlar:**
    *   `Precharge_Process_Run()`: DC bara voltajı, batarya voltajının %95'ine ulaştığında ana kontaktörü çeken algoritma. Zaman aşımı (timeout) kontrolü (Örn: 3 saniye içinde %95'e ulaşmazsa FAULT ver).
    *   `Contactor_Emergency_Open()`: Kaza, donanımsal acil stop butonu veya IMD hatası anında tüm kontaktörleri 10ms içinde düşüren kesme (interrupt) fonksiyonu.

### 3. `vcu_pedal_map.c` / `vcu_pedal_map.h`
*   **İçerik:** Gaz ve fren pedallarından gelen çift kanallı (redundant) ADC sinyallerinin okunması, filtrelenmesi ve tork talebine çevrilmesi.
*   **Kritik Fonksiyonlar:**
    *   `Read_Throttle_ADC()`: Çift potansiyometreli (veya hall effect) gaz pedalını okur. Kanal A ve Kanal B arasındaki sapma %10'u geçerse hata bayrağı (flag) çeker.
    *   `Calculate_Torque_Request()`: Sürüş moduna (Eco, Sport) göre ADC değerini Nm cinsinden hedef torka çevirir.
    *   `Brake_Plausibility_Check()`: Sürücü hem gaza hem frene aynı anda basarsa, tork talebini anında 0'a zorlayan (Brake Override) güvenlik algoritması.

### 4. `vcu_can_matrix.c` / `vcu_can_matrix.h`
*   **İçerik:** Araç içi CAN-Bus ağının (500 kbps) yönetimi. Gelen mesajların ayrıştırılması ve VCU komutlarının paketlenip gönderilmesi.
*   **Kritik Fonksiyonlar:**
    *   `CAN_RX_Interrupt()`: BMS, İnvertör ve IMD'den gelen CAN mesajlarını FIFO buffer'dan alır ve struct'lara kaydeder.
    *   `CAN_TX_Task()`: Motor sürücüye "Hedef Tork" ve "Enable" sinyallerini 10ms periyotla gönderir.

### 5. `vcu_aux_control.c` / `vcu_aux_control.h`
*   **İçerik:** Araç yardımcı sistemlerinin (Su pompaları, radyatör fanları, farlar, silecekler, sinyaller) kontrolü.
*   **Kritik Fonksiyonlar:**
    *   `Thermal_Management_Task()`: Motor veya İnvertörden gelen sıcaklık verisine göre soğutma pompasının hızını (PWM ile) ayarlar. Sıcaklık > 45°C ise fanları devreye sokar.

---

## MODÜL 2: BATARYA YÖNETİM SİSTEMİ (BMS)
**Hedef Donanım:** STM32 (BMS Master) + AFE Entegresi (Örn: LTC6811/BQ76952)
**Dil:** C (FreeRTOS)
**Görev:** Hücre voltaj/sıcaklık ölçümü, SOC/SOH hesabı, pasif balanslama ve batarya güvenliği.

### 1. `bms_afe_driver.c` / `bms_afe_driver.h`
*   **İçerik:** Analog Front End (AFE) çipi ile izole SPI (isoSPI) üzerinden haberleşme.
*   **Kritik Fonksiyonlar:**
    *   `AFE_Read_Cell_Voltages()`: Seri bağlı tüm hücrelerin (Örn: 27S) gerilimlerini mV hassasiyetinde okur.
    *   `AFE_Read_Temperatures()`: NTC sıcaklık sensörlerinin analog değerlerini sıcaklığa (°C) çevirir.

### 2. `bms_soc_algorithm.c` / `bms_soc_algorithm.h`
*   **İçerik:** Bataryanın şarj durumu (State of Charge - SOC) ve sağlık durumu (State of Health - SOH) hesaplamaları.
*   **Kritik Fonksiyonlar:**
    *   `Calculate_SOC_CoulombCounting()`: Hall effect akım sensöründen (Örn: LEM) gelen amperi zamana göre integre ederek anlık kapasiteyi hesaplar.
    *   `SOC_EKF_Correction()`: Genişletilmiş Kalman Filtresi (Extended Kalman Filter) kullanarak, açık devre voltajı (OCV) eğrisi ile Coulomb sayacındaki kaymaları (drift) düzeltir.

### 3. `bms_protection.c` / `bms_protection.h`
*   **İçerik:** Yazılımsal ve donanımsal koruma limitleri.
*   **Kritik Fonksiyonlar:**
    *   `BMS_Fault_Monitor_Task()`: Hücre voltajı > 4.2V (Aşırı Şarj), < 2.5V (Aşırı Deşarj) veya Sıcaklık > 60°C ise FAULT bayrağı çeker ve VCU'ya CAN üzerinden "Acil Güç Kes" komutu yollar.
    *   `Dynamic_Current_Limits()`: Batarya sıcaklığına ve SOC durumuna göre anlık izin verilen maksimum şarj ve deşarj akımlarını hesaplayıp VCU'ya bildirir (Torque Derating).

### 4. `bms_balancing.c` / `bms_balancing.h`
*   **İçerik:** Pasif hücre dengeleme mantığı.
*   **Kritik Fonksiyonlar:**
    *   `Balance_Logic_Update()`: Şarj esnasında, maksimum ve minimum hücre arasındaki fark > 0.05V ise, voltajı yüksek olan hücrelerin MOSFET'lerini açarak deşarj direnci üzerinden dengeleme yapar.

---

## MODÜL 3: MOTOR SÜRÜCÜ (INVERTER / FOC) & ABS
**Hedef Donanım:** STM32G4 veya TI C2000 (Donanımsal matematik hızlandırıcılı mikrodenetleyici)
**Dil:** C / C++ (Bare-metal veya RTOS)
**Görev:** DC'yi AC'ye çevirme, FOC (Alan Yönlendirmeli Kontrol), ABS ve Çekiş Kontrol.

### 1. `foc_math.c` / `foc_math.h`
*   **İçerik:** AC motor kontrolü için gereken ağır matematiksel dönüşümler.
*   **Kritik Fonksiyonlar:**
    *   `Clarke_Transform()`: 3 fazlı (a,b,c) akımları 2 eksenli (alpha, beta) sabit sisteme dönüştürür.
    *   `Park_Transform()`: Sabit sistemi, rotor ile birlikte dönen (d, q) eksenlerine dönüştürür.
    *   `Inv_Park_Transform()` & `SVPWM_Generate()`: Hesaplanan hedef voltajları Uzay Vektör PWM sinyallerine çevirerek Gate Driver çipine gönderir.

### 2. `foc_current_loop.c` / `foc_current_loop.h`
*   **İçerik:** İnvertörün en hızlı koşan (Örn: 10kHz / 100 mikro saniye) akım kontrol döngüsü.
*   **Kritik Fonksiyonlar:**
    *   `Current_Control_ISR()`: ADC'den faz akımlarını (Ia, Ib) okur, FOC matematiğini çalıştırır, PI kontrolcüler (PID_Id, PID_Iq) ile hatayı sıfırlar ve yeni PWM duty-cycle değerlerini Timer register'larına yazar.

### 3. `foc_sensors.c` / `foc_sensors.h`
*   **İçerik:** Motor pozisyon (Rotor Açı) sensörlerinin okunması.
*   **Kritik Fonksiyonlar:**
    *   `Read_Resolver()` veya `Read_Encoder()`: Motor milinin kesin elektriksel açısını hesaplar. (FOC algoritması için 0.1 derece hassasiyetle açı bilgisi şarttır).

### 4. `abs_traction_control.c` / `abs_traction_control.h`
*   **İçerik:** Frenleme ve hızlanma anında kaymayı önleyen aktif güvenlik yazılımı.
*   **Kritik Fonksiyonlar:**
    *   `Calculate_Wheel_Slip()`: Araç referans hızı ile tekerlek hızı arasındaki kayma oranını (Slip Ratio) hesaplar.
    *   `ABS_Regen_Modulator()`: Kayma oranı %20'yi aşarsa tekerlek kilitleniyor demektir. Rejeneratif frenleme torkunu (negatif Iq) anlık olarak 0'a çeker ve tekrar basar (Pulse efekti).
    *   `TCS_Torque_Limit()`: Hızlanırken patinaj varsa, VCU'dan gelen hedef torku anlık olarak kısar.

---

## MODÜL 4: TELEMETRİ VE KARA KUTU (DATA LOGGER)
**Hedef Donanım:** Raspberry Pi 4 / Compute Module 4
**Dil:** Python 3 (Threading / Asyncio)
**Görev:** Araç verilerini buluta gönderme, SD karta yazma ve GPS takibi.

### 1. `telemetry_can_parser.py`
*   **Görev:** `python-can` kütüphanesini kullanarak SPI-CAN modülünden (Örn: MCP2515) gelen RAW verileri okur. DBC dosyası kullanarak Byte'ları anlamlı verilere (Voltaj, Amper, Sıcaklık, Hata Kodu) çevirir (Decode).

### 2. `telemetry_mqtt_lora.py`
*   **Görev:** İşlenen araç verilerini (JSON formatında) uzak sunucuya aktarır.
*   **Fonksiyonlar:** İnternet (4G/LTE) varsa veriyi güvenli bir MQTT Broker'ına (AWS IoT veya özel sunucu) yayınlar (Publish). İnternet yoksa veriyi seri port (UART) üzerinden LoRa SX1278 modülüne basar.

### 3. `telemetry_gps.py`
*   **Görev:** UART üzerinden NMEA protokolünde GPS modülünü okur. Enlem, Boylam, Rakım ve gerçek araç hızını (Satelite bazlı) ayrıştırarak ana veri havuzuna ekler.

### 4. `telemetry_datalogger.py`
*   **Görev:** Kara Kutu. Saniyede 10 kez tüm kritik araç parametrelerini Unix Timestamp (Zaman damgası) ile birlikte yerel bir SQLite veritabanına veya .csv dosyasına kaydeder. Bağlantı kopmalarında veri kaybını önler.

---

## MODÜL 5: HMI (GÖSTERGE PANELİ VE KULLANICI ARAYÜZÜ)
**Hedef Donanım:** Raspberry Pi 4 + 7" veya 10" Dokunmatik Ekran
**Dil:** C++ (Qt) veya Python (PyQt / QML)
**Görev:** Sürücüye araç bilgilerini grafiksel olarak sunmak.

### 1. `hmi_main.py` (veya .cpp)
*   **Görev:** Grafik arayüz motorunu başlatır. Telemetri modülünden (IPC - Inter-Process Communication, Socket veya Redis üzerinden) gerçek zamanlı verileri alır.

### 2. `UI_Dashboard.qml` (Tasarım Katmanı)
*   **İçerik:** Arayüz bileşenleri:
    *   Dijital Hız Göstergesi (Ortada büyük font).
    *   Dinamik Güç/Regen Barı (Gaza basınca sağa yeşil, frene basınca sola mavi dolan bar).
    *   SOC Göstergesi (Batarya yüzdesi ve dinamik menzil tahmini).

### 3. `UI_Warning_Logic.py`
*   **Görev:** BMS veya VCU'dan "Hata" flag'i geldiğinde ekranın üstünde kırmızı uyarı ikonlarını (Telltales) çıkarır. (Örn: "Batarya Aşırı Isındı", "İzolasyon Hatası", "ABS Aktif").

### 4. `UI_Charging_Screen.qml`
*   **Görev:** Araç şarja takıldığında sürüş ekranını gizler, yerine şarj animasyonu, tahmini dolum süresi ve şarj gücünü (kW) gösteren özel bir arayüz çizer.

---

## MODÜL 6: YARDIMCI SİSTEMLER (OBC & IMD)
**Hedef Donanım:** STM32 (VCU içine entegre edilebilir veya ayrı mikrodenetleyici)
**Dil:** C

### 1. `obc_charger_control.c` / `obc_charger_control.h`
*   **İçerik:** Yerleşik Şarj Cihazı (On-Board Charger) yönetimi.
*   **Kritik Fonksiyonlar:** AC şebeke bağlantısı algılandığında (Pilot sinyali okuma) aracı kilitler (Sürüşü engeller). BMS'ten gelen "Maksimum İzin Verilen Şarj Akımı" bilgisini CAN üzerinden güç elektroniği şarj ünitesine ileterek CC/CV (Sabit Akım/Sabit Voltaj) şarj profilini yönetir.

### 2. `imd_insulation_monitor.c` / `imd_insulation_monitor.h`
*   **İçerik:** İzolasyon İzleme Cihazından (Örn: Bender IR155) gelen PWM veya CAN sinyalinin okunması.
*   **Kritik Fonksiyonlar:** Şasi ile Yüksek Voltaj hattı arasındaki direnç (Örn: 100 kOhm) altına düşerse, anında kesme (interrupt) üreterek `Contactor_Emergency_Open()` fonksiyonunu tetikler ve ekranda kırmızı "İzolasyon Hatası" çıkartır.