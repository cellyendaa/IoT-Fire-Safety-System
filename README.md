# 🔥 IoT Fire Safety Monitoring System (ESP32 + ThingsBoard)

Sistem monitoring kebakaran berbasis IoT menggunakan **ESP32**, yang memantau suhu, kelembapan, asap, dan cahaya secara real-time. Data dikirim ke **ThingsBoard** melalui **MQTT**, dilengkapi tampilan LCD lokal, sistem alarm, serta penyimpanan data offline.



## 📦 Fitur Utama

- 🔄 **Real-time sensor monitoring** (temperature, humidity, smoke, light)
- 🚨 **Multi-stage alert system**:
  - Indikator **RGB LED**
  - **Buzzer alarm** untuk kondisi kritis
- 📟 **LCD Display** untuk monitoring lokal
- ☁️ **Integrasi MQTT** ke ThingsBoard
- 📁 **Penyimpanan data offline** & auto-sync saat koneksi kembali
- 📶 **WiFi auto-reconnect**
- 💡 **Deep sleep mode** untuk hemat daya saat darurat
- 🖥️ **Antarmuka serial** untuk perintah debugging



## ⚙️ Perangkat & Library

- **Board**: ESP32 (Wokwi-compatible)
- **Sensor Simulasi**: suhu, kelembapan, asap, cahaya
- **Komponen Output**: RGB LED, Buzzer, LCD I2C

### Library yang digunakan:
- `WiFi.h`
- `PubSubClient.h`
- `ArduinoJson.h`
- `Wire.h`
- `LiquidCrystal_I2C.h`
- `Preferences.h`



## 🌐 Koneksi ThingsBoard

- **MQTT Server**: `thingsboard.cloud`
- **Port**: `1883`
- **Username (Access Token)**: `7machkmkxfg369m89v0m`



## 📁 Struktur File

| File | Deskripsi |
|------|-----------|
| `sketch.ino` | Kode utama proyek ESP32 |
| `diagram.json` | Rangkaian simulasi di Wokwi |
| `libraries.txt` | Dependensi pustaka |
| `wokwi-project.txt` | Metadata Wokwi Project |



## 🧪 Simulasi via Wokwi

1. Buka [https://wokwi.com](https://wokwi.com)
2. Unggah file `sketch.ino` dan `diagram.json`
3. Jalankan simulasi, pantau output via **Serial Monitor** dan LCD
4. Coba perintah serial: `reset`, `status`, `simulate_offline`


## 📤 Deploy ke Board ESP32

Jika tidak menggunakan Wokwi:
1. Flash file `sketch.ino` ke ESP32 via Arduino IDE
2. Pastikan koneksi WiFi dan token ThingsBoard benar
3. Buka Serial Monitor untuk melihat log


## 📬 License

This project is licensed under the MIT License — feel free to use and modify with credit.
