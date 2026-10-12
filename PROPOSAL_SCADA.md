# PROPOSAL AKHIR DESAIN PROYEK DEPARTEMEN TEKNIK ELEKTRO
## SCaDA: Smart Cuff and Distress Alarm

**Kelompok 23**
- **Azka Nabihan Hilmy** (Teknik Komputer - 2306250541)
- **Fadlin Alwan Hanafi Hadibrata** (Teknik Elektro - 2306224335)
- **Muhammad Gavin Jericho** (Teknik Elektro - 2306215002)
- **Argatha Advelida Hutagalung** (Teknik Biomedik - 2306168945)
- **Rafandra Gifarrel Maritza Alifa** (Teknik Elektro - 2306216390)

**Klien:** Suku Dinas Penanggulangan Kebakaran dan Penyelamatan Jakarta Timur  
**Institusi:** Departemen Teknik Elektro, Fakultas Teknik Universitas Indonesia  
**Periode:** Juni 2026  

---

## DAFTAR ISI
1. [BAB 1 - PENDAHULUAN](#bab-1---pendahuluan)
   - 1.1 Motivasi & Dampak
   - 1.2 Tinjauan Perangkat & Metode yang Ada
   - 1.3 Alternatif Solusi
   - 1.4 Pernyataan Masalah
2. [BAB 2 - TINJAUAN PUSTAKA, TEORI DASAR, STANDAR & BATASAN](#bab-2---tinjauan-pustaka-teori-dasar-standar--batasan)
   - 2.1 Latar Belakang & Kebutuhan Pengguna
   - 2.2 Tinjauan Pustaka & Technology Gap
   - 2.3 Teori Dasar & Mata Kuliah Terkait
   - 2.4 Standar Rekayasa & Batasan Realistis
3. [BAB 3 - DESAIN SISTEM](#bab-3---desain-sistem)
   - 3.1 Desain Awal & Perancangan Sistem (PDS, Arsitektur, Komponen)
   - 3.2 Kompleksitas Desain Sistem (RTOS, Motion Artifact, PASS FSM, Star-Mesh, TinyML)
4. [BAB 4 - EVALUASI DAN KELAYAKAN DESAIN](#bab-4---evaluasi-dan-kelayakan-desain)
   - 4.1 Evaluasi Desain Awal (RTM, Analisis Termal & Daya, Kapasitas Saluran, Edge DSP, IMU)
   - 4.2 Studi Kelayakan Desain Akhir (Operasional, Ergonomi, Adopsi, Skalabilitas)
5. [BAB 5 - PERENCANAAN PROYEK](#bab-5---perencanaan-proyek)
   - 5.1 Perencanaan Kerja, Timeline & Tanggung Jawab Tim
   - 5.2 Rancangan Anggaran Biaya (RAB)
   - 5.3 Business Model Canvas (BMC)
6. [LAMPIRAN & REFERENSI](#lampiran--referensi)

---

## ABSTRACT
Firefighters are exposed not only to external hazards such as fire, smoke, and structural collapse, but also to physiological risks that are difficult to observe directly during operations. SCaDA (Smart Cuff and Distress Alarm) is proposed as a wearable arm cuff system designed to monitor firefighters’ vital signs and distress conditions in real time. The system integrates MAX30102 for heart rate, SpO₂, and respiratory rate estimation, MAX30205 for skin/contact temperature, and BMI160 for motion and man-down detection. Sensor data are processed by an ESP32 using signal processing and TinyML-based risk classification to determine user status as Safe, Warning, or Danger. Data and emergency alerts are transmitted using ZigBee and LoRa to support command center monitoring. SCaDA is expected to assist commanders in making data-based decisions for rotation, rest, or evacuation.

*Keywords: Firefighter, Smart Cuff, Vital Sign Monitoring, TinyML, Distress Alarm*

## ABSTRAK
Pemadam kebakaran tidak hanya menghadapi bahaya eksternal seperti api, asap, dan runtuhan bangunan, tetapi juga risiko fisiologis yang sulit diamati secara langsung selama operasi. SCaDA (Smart Cuff and Distress Alarm) diusulkan sebagai sistem wearable berbentuk arm cuff untuk memantau tanda vital dan kondisi darurat personel secara real-time. Sistem ini mengintegrasikan MAX30102 untuk detak jantung, SpO₂, dan estimasi laju pernapasan, MAX30205 untuk suhu kulit/kontak, serta BMI160 untuk deteksi gerak dan man-down. Data sensor diproses oleh ESP32 melalui pengolahan sinyal dan klasifikasi risiko berbasis TinyML untuk menentukan status Aman, Waspada, atau Bahaya. Data dan peringatan darurat dikirim menggunakan ZigBee dan LoRa untuk mendukung pemantauan command center. SCaDA diharapkan membantu komandan mengambil keputusan rotasi, istirahat, atau evakuasi berbasis data.

*Kata Kunci: Pemadam Kebakaran, Smart Cuff, Pemantauan Tanda Vital, TinyML, Distress Alarm*

---

## BAB 1 - PENDAHULUAN

### 1.1 Motivasi & Dampak
Pemadam kebakaran merupakan profesi berisiko keselamatan tinggi. Data NFPA dan USFA (2016–2020) mencatat lebih dari 60% kematian petugas disebabkan oleh faktor kardiovaskular dan stres fisiologis (henti jantung mendadak menyumbang 45%), bukan luka bakar langsung.

Pakaian pelindung diri (APD) standar NFPA 1971 berbahan serat aramid Nomex efektif menahan radiasi panas eksternal, namun memerangkap panas metabolik tubuh sehingga memicu lonjakan *core temperature*. Risiko *heat stroke* paling kritis bahkan sering terjadi pada fase pendinginan pasca-operasi karena panas APD terus terlepas ke tubuh.

Tanpa pemantauan objektif real-time, komandan insiden hanya mengandalkan pengamatan visual subjektif untuk rotasi petugas, yang rawan memicu kolaps mendadak.

### 1.2 Tinjauan Perangkat & Metode yang Ada
1. **MSA LUNAR**: Perangkat genggam dengan *thermal imaging* (F.A.S.T), cloud FireGrid, dan man-down alarm. Namun, **tidak memiliki sensor biometrik** (HR/SpO₂).
2. **Dräger FPS 7000**: Masker SCBA dengan HUD untuk telemetri tekanan tabung dan konsumsi oksigen mekanis, tanpa evaluasi fisiologis tubuh intrinsik.
3. **Kelemahan Utama**: Berupa modul tempelan eksternal, tidak terintegrasi dengan data biometrik, menciptakan *blind spot* terhadap *heat stress* dan desaturasi oksigen.

### 1.3 Alternatif Solusi & Kesenjangan
Pasar didominasi modul eksternal navigasi. Belum ada APD terintegrasi pemantauan biometrik intrinsik yang terstandardisasi NFPA 1971. SCaDA mengisi *gap* ini.

### 1.4 Pernyataan Masalah
1. Bagaimana merancang perangkat wearable pemantau tanda vital real-time, non-invasif, dan berkelanjutan tanpa mengganggu mobilitas?
2. Bagaimana membangun mekanisme *distress alarm* andal (otomatis via henti gerak/manual) dengan transmisi ke komandan secara instan?
3. Bagaimana mengintegrasikan komunikasi nirkabel LoRa/ZigBee agar data biometrik dan SOS sampai ke dashboard komandan pada lingkungan ekstrem/NLOS?

---

## BAB 2 - TINJAUAN PUSTAKA, TEORI DASAR, STANDAR & BATASAN

### 2.1 Latar Belakang & Kebutuhan Pengguna (Klien: Sudin Damkar Jaktim)
- Damkar Jaktim saat ini menggunakan DSU (*Distress Signal Unit*) mekanis/akustik yang berbunyi jika diam ~20 detik.
- Kebutuhan riil: Pemantauan fisiologis objektif (HR, SpO₂, suhu, RR) untuk rotasi regu dan evakuasi dini.
- Masukan pengguna: Bentuk *arm strap / cuff* di lengan/tangan agar ringan dan tidak membebani APD (~20 kg).

### 2.2 Parameter & Sensor yang Digunakan
| Parameter | Sensor | Fungsi pada SCaDA |
|---|---|---|
| **Heart Rate (HR)** | MAX30102 (PPG) | Menilai beban kerja jantung & stres fisik |
| **SpO₂** | MAX30102 (Pulse Oximetry) | Memantau saturasi oksigen darah |
| **Respiratory Rate (RR)** | Estimasi variasi sinyal PPG (RSA) | Memperkirakan laju napas (0.1–0.5 Hz) tanpa *chest belt* |
| **Suhu Tubuh / Kulit** | MAX30205 (Skin contact) | Memantau perubahan suhu termal (akurasi ±0.1°C) |
| **Gerakan & Orientasi** | BMI160 IMU 6-DOF | Deteksi aktivitas, diam taktis, dan *man-down* |

### 2.3 Teori Pengolahan Sinyal & Klasifikasi
- **PPG Filtering**: Butterworth Bandpass Filter (0.5–4.0 Hz) untuk isolasi HR.
- **Adaptive Noise Cancellation (ANC)**: Sensor fusion antara IMU BMI160 dan PPG MAX30102 untuk mitigasi *motion artifact*.
- **Signal Quality Gatekeeper**: *Perfusion Index* (PI = AC/DC × 100%). Hard limit $PI_{min} = 0.4\%$. Jika sinyal buruk akibat vasokonstriksi atau guncangan, data ditolak (*data drop*) dan sistem menggunakan *Forward Fill / Linear Interpolation* pada *sliding window 60 detik* guna mencegah *false alarm*.
- **Limitasi Fisiologis Estimasi Laju Napas (RSA)**: Estimasi *Respiratory Sinus Arrhythmia* efektif pada rentang istirahat s.d. kerja moderat ($HR \le 150\text{ bpm}$). Pada kondisi takikardia ekstrem ($HR > 160\text{ bpm}$ saat memadamkan api), variabilitas modulasi napas tereduksi sehingga sistem memprioritaskan parameter detak jantung dan suhu inti untuk penilaian risiko.
- **TinyML Risk Classification**: Model TFLM INT8 terkuantisasi (PTQ) pada ESP32 (SRAM 520 KB). Mengklasifikasikan status 3 level: **Aman, Waspada, Bahaya**. Inferensi < 10 ms.
- **Physiological Strain Index (PSI)**: Indeks stres fisiologis Moran et al. (1998):
  $$PSI = 5 \cdot \frac{HR_t - HR_0}{HR_{max} - HR_0} + 5 \cdot \frac{T_{core,t} - T_{core,0}}{39.5 - T_{core,0}}$$
  Rentang: Normal (0–2), Waspada (3–6), Berbahaya (7–10).

### 2.4 Standar Rekayasa & Batasan Realistis
- **NFPA 1982 (Standard on Personal Alert Safety Systems / PASS)**: Standar wajib untuk sistem alarm keselamatan personel damkar, menetapkan ambang pra-alarm henti gerak 15–20 detik (akustik $\ge 60\text{ dB}$), alarm penuh pada 30 detik tanpa respons (sirene $\ge 90\text{ dB}$ pada jarak 3 meter), serta tombol pembatalan manual.
- **NFPA 1971**: Standar pakaian pelindung APD; kabel interkoneksi tidak boleh merusak *moisture barrier* atau mendegradasi TPP (Thermal Protective Performance) $> -5\%$.
- **IEC 60529 IP67**: Kedap debu dan tahan rendaman air 1 meter 30 menit.
- **Regulasi ISM Band**: Frekuensi 915 MHz (LoRa) mematuhi batasan *Duty Cycle* 1% pada pita frekuensi darurat yang tidak mengganggu frekuensi radio VHF/UHF damkar.
- **Batasan Ergonomi**: Berat total sistem wearable $\le 500$ gram.

---

## BAB 3 - DESAIN SISTEM

### 3.1 Target Spesifikasi Desain (PDS)
- Akurasi HR: Deviasi $\le 10\%$ saat bergerak.
- Akurasi SpO₂: Deviasi $\le 2\%$ kondisi stasioner.
- Akurasi Suhu: $\pm 0.1^\circ\text{C}$ (37–39°C, ASTM E1112).
- Latensi Peringatan SOS: $\le 3$ detik.
- Packet Loss Rate: $\le 5\%$ di lingkungan gedung simulasi.
- Jangkauan LoRa NLOS: $\ge 150$ m menembus beton bertulang.
- Total BOM: $\le \text{Rp } 2.000.000$.

#### Tabel 3.1.A Alokasi Pinout GPIO Definitif ESP32 DevKit V1
| Pin ESP32 | Tipe Interface | Komponen Terhubung | Fungsi / Keterangan |
|:---:|:---:|:---|:---|
| **GPIO 21** | I²C (SDA) | MAX30102, MAX30205, BMI160 | Jalur Data Bersama (Pull-up 4.7 kΩ) |
| **GPIO 22** | I²C (SCL) | MAX30102, MAX30205, BMI160 | Jalur Clock Bersama (Pull-up 4.7 kΩ) |
| **GPIO 4** | GPIO In (Ext Interrupt) | BMI160 (`INT1`) | Hardware No-Motion / Any-Motion Trigger |
| **GPIO 18** | Hardware SPI (SCK) | Modul LoRa Ra-02 (SX1276) | SPI Bus Clock |
| **GPIO 19** | Hardware SPI (MISO) | Modul LoRa Ra-02 (SX1276) | SPI Master In Slave Out |
| **GPIO 23** | Hardware SPI (MOSI) | Modul LoRa Ra-02 (SX1276) | SPI Master Out Slave In |
| **GPIO 5** | GPIO Out (Chip Select) | Modul LoRa Ra-02 (`NSS`) | SPI Slave Select |
| **GPIO 14** | GPIO Out (Reset) | Modul LoRa Ra-02 (`RST`) | Radio Reset |
| **GPIO 2** | GPIO In (Interrupt) | Modul LoRa Ra-02 (`DIO0`)| Packet RX/TX Done Interrupt |
| **GPIO 25** | LEDC PWM Out | Buzzer Piezo Darurat | Pre-Alarm (60 dB) & Sirene (90 dB) |
| **GPIO 26** | GPIO Out (MOSFET) | LED Strobo 5W High Brightness | Visual Distress Strobe Light |
| **GPIO 27** | GPIO In (Pull-up) | Tombol SOS Manual Tahan Air | Panic Button (Debounced) |
| **GPIO 16** | Hardware UART2 (RX) | Modul GPS NEO-6M (`TX`) | NMEA Coordinates Ingest |
| **GPIO 17** | Hardware UART2 (TX) | Modul GPS NEO-6M (`RX`) | NMEA Configuration (Optional) |

### 3.2 Arsitektur Sistem: *Distributed Embedded System* (Alternatif C - Dipilih)
Sistem dibagi menjadi 2 modul fisik terpisah:
1. **Strap Sensor (Wearable Arm Band)**:
   - Terpasang di pergelangan / lengan bawah pemadam.
   - Modul PCB custom tipis: MAX30102 (PPG) + MAX30205 (Suhu Kontak Medis).
   - Bahan: Kain Neoprene tahan panas dengan pengunci Velcro.
   - Dimensi $\le 30 \times 20 \times 6$ mm, massa ~20 g.
2. **Distress Signal Unit (DSU)**:
   - Terpasang di punggung atas / SCBA strap (area terlindung).
   - ESP32 MCU, LoRa Ra-02 (915 MHz), BMI160 IMU, GPS NEO-6M, Baterai 4x AAA NiMH (4.8V) + Buck Converter MP1584, Sirene Piezo 90 dB, Strobo LED 5W, Tombol SOS.
   - Enkapsulasi IP67 Bahan PETG tahan panas ($T_g \approx 80\text{--}85^\circ\text{C}$).
3. **Kabel Interkoneksi Lengan-ke-Punggung**:
   - Kabel 4-kawat fleksibel AWG 26 (3.3V, GND, SDA, SCL) panjang 1.2 m.
   - Dilengkapi **Konektor Lepas-Cepat Tahan Air (Quick-Release Connector JST-JWPF / M8 IP67)** di sambungan DSU untuk memudahkan pelepasan jaket dan pencucian dekontaminasi.

### 3.3 Kompleksitas Desain Sistem
- **FreeRTOS Asymmetric Multithreading**:
  - **Core 0 (PRO_CPU)**: Mengisolasi komunikasi radio LoRa (Wi-Fi/BLE dimatikan total demi efisiensi baterai).
  - **Core 1 (APP_CPU)**: Deterministic hard real-time (I²C sampling 100 Hz via Hardware Timer ISR, DSP filter, FSM PASS, inferensi TinyML INT8).
  - **Non-Blocking IPC**: FreeRTOS Queue (Ring Buffer thread-safe).
- **PASS FSM 3-Tahap (Sesuai NFPA 1982)**:
  - *State 0 (Normal)*: Vektor gerak $> 0.1\text{G}$, timer reset ke 0.
  - *State 1 (Pre-Alarm / 15 detik diam)*: Buzzer berbunyi *intermittent chirp* lembut (~60 dB). Reset instan jika pemadam menggerakkan lengan.
  - *State 2 (Full Distress / 30 detik tanpa respons)*: Sirene darurat 90 dB kontinu + Strobo LED + transmisi radio SOS Priority Preemption ke posko.
- **Graceful Degradation QoS**:
  - *Data Throttling*: Base-Station Polling terjadwal (Time-slot ~3s per node, siklus 15s untuk 5 pemadam).
  - *Priority Preemption*: Sinyal SOS darurat langsung menyela antrean polling dengan broadcast 3x instan.

---

## BAB 4 - EVALUASI DAN KELAYAKAN DESAIN

### 4.1 Requirements Traceability Matrix (RTM)
- **REQ-01 (Termal Ekstrem 85°C)**: LTO / LiFePO4 battery chemistry (thermal breakdown $>90^\circ\text{C}$), potting compound isolasi PCB.
- **REQ-02 (Akurasi Biosinyal)**: ANC IMU + PPG fusion, hard-limit $PI > 0.4\%$, sampling 100 Hz hardware timer.
- **REQ-03 (Stabilitas Jaringan NLOS)**: Star-Mesh (ZigBee lokal antar personel + LoRa jarak jauh dari komandan ke posko).
- **REQ-04 (Integritas PASS)**: BMI160 zero-g drift $\pm 0.4\text{ mg/K}$ dengan FSM 3-fase & gerbang logika AND biometrik.
- **REQ-05 (Latensi Transmisi Darurat $\le 3$s)**: FreeRTOS priority preemption (task switching latency skala mikrodetik).

### 4.2 Analisis Daya & Duty Cycle LoRa
- **Worst-case current draw**: ESP32 (~240 mA), MAX30102 (~20 mA), Haptic motor (~80 mA), BMI160/MAX30205 (~2 mA), Buck converter loss 10% (~34 mA) $\rightarrow$ Total $I_{peak} \approx 376\text{ mA}$.
- **Durasi Baterai**: Untuk baterai LTO 800 mAh, $t = (800 / 376) \times 0.85 \approx 1.8\text{ jam (108 menit)}$ (memenuhi syarat operasi SCBA 30–60 menit).
- **LoRa Duty Cycle 1%**: Transmisi payload 30-byte (5 personel) SF9 BW125kHz membutuhkan ToA 164 ms $\rightarrow T_{wait} = 164\text{ ms} \times 99 = 16.2\text{ detik}$. Interval transmisi 20–30s aman dari batas regulasi ISM.

### 4.3 Design Matrix (Vest vs Arm Cuff)
- **Arm Cuff**: Total weighted score **92/100** (unggul pada ergonomi, tidak membebani APD 20 kg, mudah lepas-pasang, lolos evaluasi risiko).
- **Vest**: Total weighted score **78/100** (menambah beban panas dan berat, sulit dekontaminasi karsinogen).

---

## BAB 5 - PERENCANAAN PROYEK & ANGGARAN

### 5.1 Pembagian Tanggung Jawab Tim (Despro 1 & 2)
| Anggota | Peran | Alokasi Jam | Fokus Utama |
|---|---|---|---|
| **Argatha Advelida Hutagalung** | Project Manager & Integrasi Sistem | 62 jam | Timeline, dokumen, integrasi multi-subsistem |
| **Fadlin Alwan Hanafi Hadibrata** | Hardware Engineer | 55 jam | Skematik PCB, power management, solder SMD, kalibrasi sensor (MAX30102, MAX30205, BMI160) |
| **Azka Nabihan Hilmy** | Firmware & Embedded Systems Engineer | 56 jam | ESP32 FreeRTOS, driver I²C sensor, TinyML edge deployment, LoRa |
| **Muhammad Gavin Jericho** | Machine Learning & Data Engineer | 50 jam | Dataset fisiologis, training model PSI, TFLM quantization INT8 |
| **Rafandra Gifarrel Maritza Alifa** | Mechanical Design & UI Engineer | 54 jam | Fusion 360 3D casing IP67, mounting arm cuff, Web/Mobile Dashboard |

### 5.2 Rencana Anggaran Biaya (RAB) Prototipe
Total anggaran belanja purwarupa dirancang sebesar **Rp 1.591.000** (dibawah pagu maksimal Rp 2.000.000), mencakup:
- MAX30102 PPG Optical Sensor (2x): Rp 70.000
- MAX30205 Human Body Temp Sensor (2x): Rp 90.000
- Modul IMU BMI160 6-DOF (2x): Rp 60.000
- ESP32 DevKit V1 (3x): Rp 225.000
- Modul LoRa Ra-02 SX1276 (3x): Rp 195.000
- Buzzer / Sirene Piezo (2x): Rp 40.000
- LED Strobo 5W High Brightness (2x): Rp 50.000
- Tombol SOS Waterproof IP65 (2x): Rp 30.000
- Baterai AAA NiMH Rechargeable 1250mAh DoublePow (8 pcs / 2 set): Rp 80.000
- Battery Holder 4x AAA dengan Saklar On/Off (2x): Rp 30.000
- Modul Step-Down Buck Converter DC-DC MP1584 3.3V (2x): Rp 30.000
- Casing 3D Printing (PETG) + Bahan Strap Neoprene + Aksesoris/Kabel/Solder: Rp 625.000

---

## 5.3 Business Model Canvas (BMC)
- **Key Partners**: Dinas Pemadam Kebakaran & Sudin Damkar Jaktim, Vendor APD, Produsen PCB, Lembaga Sertifikasi/Pengujian.
- **Key Activities**: Riset & validasi lapangan, desain/fabrikasi cuff & PCB, integrasi biosinyal & TinyML, perancangan dashboard komando, uji simulasi kebakaran, layanan kalibrasi/maintenance.
- **Key Resources**: Sensor biometrik (MAX30102, MAX30205), IMU (BMI160), MCU ESP32, modul radio LoRa, IP/Algoritma PSI TinyML, SDM multi-disiplin (Komputer, Elektro, Biomedik).
- **Value Propositions**: Pemantauan objektif real-time tanda vital (HR, SpO2, Temp, RR), deteksi dini heat stress & kolaps, alarm distres otomatis/manual (PASS), bentuk arm cuff ergonomis tanpa modifikasi APD.
- **Customer Relationships**: Training komandan & personel, customer service lapangan, kalibrasi/maintenance berkala.
- **Channels**: Penjualan langsung B2G ke Dinas Pemadam Kebakaran kab/kota, kemitraan bundling dengan vendor APD.
- **Customer Segments**: Dinas Damkar Indonesia (prioritas: DKI Jakarta, Surabaya, kota industri padat).
- **Cost Structure**: R&D, komponen hardware & fabrikasi PCB/casing, hosting/server dashboard, sertifikasi standar keselamatan.
- **Revenue Streams**: Penjualan paket unit SCaDA (Cuff + DSU + Dashboard Gateway), kontrak pemeliharaan & kalibrasi tahunan.

---

## 7. ADDENDUM: REVISI ARSITEKTUR SISTEM (PERSIAPAN DESPRO 2)
*Disepakati pada review teknis persiapan implementasi prototipe:*

1. **Topologi Komunikasi Nirkabel: *Direct LoRa Star Topology***
   - **Penyederhanaan**: Menghilangkan perantara unit *Gateway Komandan* fisik dan radio lokal *ZigBee/ESP-NOW*.
   - **Aliran Data**: Seluruh unit DSU di punggung pemadam memancarkan telemetri 8-byte langsung ke **Receiver USB LoRa (ESP32)** yang terhubung ke Laptop Dashboard Command Center di luar gedung via pita **915 MHz**.
   - **Manajemen Transmisi**: Menggunakan skema *Base-Station Polling* terjadwal (interval ~3 detik per node, siklus 15 detik untuk 5 pemadam) dengan okupansi kanal udara hanya **2.06%** dan *duty cycle* **0.41%** (lolos regulasi ISM $<1.0\%$).
   - **Darurat**: Sinyal SOS / PASS menggunakan mode *Priority Preemption Broadcast* 3x tanpa menunggu antrean polling.

2. **Dampak Efisiensi**:
   - Radio Wi-Fi dan Bluetooth pada ESP32 DevKit V1 dinonaktifkan total (`WiFi.mode(WIFI_OFF)`), menghemat konsumsi arus baterai ~30–40%.
   - Firmware ESP32 menjadi 100% seragam untuk semua unit wearable.
   - Mengeliminasi *Single Point of Failure* di lapangan.

3. **Penyederhanaan Aktuator Haptik & Mekanisme Pre-Alarm (KISS & Standar PASS Komersial)**:
   - **Eliminasi Motor Getar**: *Vibration motor* dihilangkan sepenuhnya dari sistem. Keputusan ini mencegah risiko *noise/back-EMF* pada bus I²C di kabel sensor lengan, menghilangkan part mekanik bergerak yang rentan rusak, serta menghemat beban arus ~80 mA.
   - **Pre-Alarm Akustik Bertingkat (PWM Buzzer)**:
     - *State 1 (Pre-Alarm 15s)*: Buzzer piezo mengeluarkan bunyi *intermittent chirp* lembut (~60 dB) via modulasi PWM. Pemadam cukup menggerakkan badan/tangan untuk membatalkannya.
     - *State 2 (Full Distress 30s)*: Buzzer piezo memancarkan sirene darurat maksimal **90 dB** kontinu + Strobo LED + transmisi radio SOS.
   - **Strap Sensor Lengan Lebih Bersih**: Modul di lengan murni hanya memuat **MAX30102 + MAX30205** dengan interkoneksi 4 kabel I²C standar (3.3V, GND, SDA, SCL).

4. **Standardisasi Sensor Inersia & PASS: BMI160 (Bosch)**:
   - **Keputusan**: Seluruh referensi MPU-6050 resmi digantikan oleh **BMI160 (6-DOF IMU)**.
   - **Alasan Rekayasa**: Stabilitas *zero-g thermal drift* ($\pm 0.4\text{ mg/K}$) pada suhu operasional mikroklimat jaket hingga 85°C serta adanya *Hardware No-Motion Interrupt Engine* internal yang membebaskan komputasi CPU ESP32.

5. **Arsitektur Catu Daya: 4x Baterai AAA NiMH Seri (4.8V) + Buck Converter**:
   - **Keputusan**: Menggunakan **4 buah Baterai AAA NiMH Rechargeable 1250 mAh dirangkai SERI (4.8V nominal)** dengan sistem *swappable battery*.
   - **Alasan Rekayasa**: Menjamin keamanan kimia mutlak (*non-flammable / zero thermal runaway risk* di dalam jaket pemadam). Tegangan 4.8V diturunkan secara efisien ke 3.3V stabil menggunakan switching *Step-Down Buck Converter (MP1584)*. Modul charger internal TP4056 dihapus dan digantikan sistem pengisian eksternal / penggantian baterai cepat (*hot-swap*).

6. **Pipeline Edge AI (TinyML): Arsitektur Hybrid (Formula Medis Moran PSI + Prediksi Tren TFLM INT8)**:
   - **Keputusan**: Mengadopsi sistem 2 lapis:
     - *Lapis 1 (Deterministik Fisiologis Real-Time)*: Menggunakan formula baku **Physiological Strain Index (PSI)** dari *Moran et al. (1998)* pada skala 0–10 untuk menentukan status saat ini: **Aman (0–2), Waspada (3–6), Bahaya (7–10)**.
     - *Lapis 2 (Prediksi Tren Kelelahan Masa Depan via TinyML)*: Model TFLM INT8 (kuantisasi pasca-latih) yang dilatih menggunakan dataset biosignal publik (*WESAD / PPG-DaLiA*) pada *sliding window 60 detik* untuk memprediksi risiko lonjakan kelelahan / heat stress 10–15 menit ke depan (*Predictive Early Warning*).
   - **Alasan Rekayasa**: Menjamin validitas klinis mutlak yang tidak bisa disanggah penguji (berdasarkan jurnal kedokteran fisiologi baku), sekaligus mempertahankan nilai inovasi Machine Learning di ESP32.

7. **Standardisasi Material Mekanik: Filamen PETG (3D Print Casing) & Kain Neoprene (Strap)**:
   - **Keputusan**: 
     - Seluruh casing 3D print (kotak DSU dan modul sensor) dicetak menggunakan **Filamen PETG** (menggantikan PLA).
     - Bahan manset lengan distandarkan 100% menggunakan **Kain Neoprene elastis dengan pengunci Velcro**.
   - **Alasan Rekayasa**: 
     - Filamen PETG memiliki *Glass Transition Temperature* ($T_g \approx 80\text{--}85^\circ\text{C}$), sehingga casing tidak akan melembek atau bocor segel IP67 saat terpapar mikroklimat internal APD pemadam kebakaran (berbeda dengan PLA yang melembek di suhu 55–60°C).
     - Kain Neoprene memberikan bantalan empuk, tahan panas, menekan sensor PPG secara stabil tanpa menghambat sirkulasi darah, serta mudah dicuci/didekontaminasi karsinogen pasca-operasi.






