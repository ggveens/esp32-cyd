# 🚀 ESP32 Cheap Yellow Display (CYD) – Smart TFT IoT Interface

![ESP32 CYD](https://randomnerdtutorials.com/wp-content/uploads/2023/09/ESP32-Cheap-Yellow-Display-Board-CYD-ESP32-2432S028R.jpg)

> A powerful all-in-one ESP32 development board with integrated touchscreen display – designed for building modern IoT interfaces.

---

## 📌 Overview

The **ESP32 Cheap Yellow Display (CYD)**, also known as **ESP32-2432S028R**, is a compact and cost-effective development board that combines an **ESP32 microcontroller** with a **2.8-inch TFT touchscreen display**.

This board is widely adopted in the maker community for creating:

* Smart home dashboards
* IoT control panels
* Embedded GUI systems
* Real-time monitoring interfaces

Unlike traditional setups that require separate ESP32 + display wiring, the CYD integrates everything into a single, ready-to-use platform.

📖 Reference: Random Nerd Tutorials ([Random Nerd Tutorials][1])

---

## ✨ Key Features

* ⚡ **ESP32-WROOM-32 dual-core MCU**
* 📶 Integrated **Wi-Fi & Bluetooth**
* 🖥️ **2.8” TFT LCD Touchscreen (ILI9341)**
* 🎯 Resolution: **240 × 320 pixels**
* 💾 **MicroSD card support**
* 🌈 Built-in **RGB LED**
* 🔌 Multiple GPIO expansion pins
* 🔋 Powered via **5V USB**
* 🧠 Supports:

  * Arduino IDE
  * MicroPython
  * ESP-IDF

---

## 📊 Specifications

| Feature      | Details                        |
| ------------ | ------------------------------ |
| MCU          | ESP32 Dual-core (up to 240MHz) |
| RAM          | 520KB SRAM                     |
| Flash        | 4MB                            |
| Display      | 2.8" TFT (ILI9341)             |
| Resolution   | 240x320                        |
| Touch        | Resistive                      |
| Connectivity | Wi-Fi + Bluetooth              |
| Power        | 5V                             |
| Size         | 50 × 86 mm                     |

---

## 🧩 Hardware Overview

The CYD board integrates multiple components into a single module:

* 📺 TFT display + touchscreen (SPI communication)
* 💡 RGB LED (GPIO 4, 16, 17)
* 💾 MicroSD card interface
* 🔌 Extended GPIO pins (GPIO 21, 22, 27, 35...)
* 🔗 Serial communication (TX/RX)

👉 This integration makes it **ideal for GUI-based embedded systems** without complex wiring. ([Random Nerd Tutorials][1])

---

## 🛠️ Getting Started

### 1. Requirements

* Arduino IDE / PlatformIO
* USB cable
* ESP32 board package installed

---

### 2. Required Libraries

Install the following libraries:

* `TFT_eSPI`
* `XPT2046_Touchscreen`

These libraries enable communication with the display and touchscreen via SPI. ([Random Nerd Tutorials][1])

---

### 3. Configuration

You must configure the `User_Setup.h` file in **TFT_eSPI** correctly to match CYD pinout.

> ⚠️ Using incorrect config will result in blank screen or incorrect display.

---

### 4. Upload First Example

Basic test:

* Display text on screen
* Detect touch input (X, Y, Pressure)

Expected result:

* Screen shows “Hello World”
* Touch displays coordinates

---

## 💡 Use Cases

* 🏠 Smart Home Control Panel
* 📊 Sensor Dashboard (Temperature, MQTT, IoT)
* 🎵 Media Controller / MP3 Player
* 📡 WiFi Configuration UI
* 🧠 Embedded GUI with LVGL

---

## 🚀 Advantages

✔️ All-in-one design (ESP32 + Display)
✔️ Very low cost
✔️ Easy to prototype GUI applications
✔️ Strong community support
✔️ Compatible with multiple frameworks

---

## ⚠️ Limitations

* Resistive touchscreen (not as smooth as capacitive)
* Limited GPIO compared to standalone ESP32
* Requires correct display configuration

---

## 📁 Project Structure (Recommended)

```
esp32-cyd/
├── src/
├── include/
├── lib/
├── data/
├── README.md
├── .gitignore
```

---

## 📚 Resources

* 📖 Official Guide: https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/
* 📦 TFT Library: https://github.com/Bodmer/TFT_eSPI
* 🧠 Touch Library: https://github.com/PaulStoffregen/XPT2046_Touchscreen

---

## 🤝 Contributing

Contributions are welcome!

* Fork the repository
* Create a new branch
* Submit a Pull Request

---

## 📜 License

This project is open-source and available under the **MIT License**.

---

## ⭐ Support

If you find this project useful:

* ⭐ Star the repository
* 🍴 Fork it
* 📢 Share it with the community

---

## 👨‍💻 Author

Developed by **[GGveens]**

---

> “Small board, big possibilities.” 🚀
