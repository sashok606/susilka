#pragma once
#include <Arduino.h>

// ===== Піни =====
struct PinsConfig {
  int fan1_pin = -1;
  int fan2_pin = -1;
  int fan3_pin = -1;
  int ssr1_pin = -1;
  int ssr2_pin = -1;

  // Датчики
  int onewire_pin = 4;   // DS18B20

  // I2C дві шини (SHT31)
  int i2c0_sda = 8, i2c0_scl = 9;
  int i2c1_sda = 10, i2c1_scl = 11;

  // PZEM-004T v3.0 (UART)
  int pzem_rx = 6;  // RX MCU (до TX PZEM)
  int pzem_tx = 5;  // TX MCU (до RX PZEM)

  // АЛІАСИ для сумісності
  int fan1_pwm = fan1_pin;  // піддув (PWM)
  int fan2_pwm = fan2_pin;  // рецирк (PWM)
  int fan3_pwm = fan3_pin;  // витяжка (PWM)
  int ssr1     = ssr1_pin;  // нагрів
  int ssr2     = ssr2_pin;  // дод. реле
};

// ===== Енергетика =====
struct EnergyConfig {
  float price_per_kwh = 6.00f; // грн/кВт·год
};

// ===== VPN (WireGuard) =====
struct VPNConfig {
  bool  enabled = false;
  char  private_key[64]     = {0};  // клієнтський приватний ключ (base64)
  char  peer_public_key[64] = {0};  // публічний ключ сервера
  char  endpoint_host[64]   = {0};  // vpn.example.com
  uint16_t endpoint_port    = 51820;
  char  address_cidr[32]    = {0};  // напр. "10.8.0.2/32"
  char  dns[32]             = {0};  // опційно "1.1.1.1"
};

// ===== Головний конфіг =====
struct AppConfig {
  // Wi-Fi
  char sta_ssid[32]{};
  char sta_pass[64]{};
  char ap_ssid[32]{};
  char ap_pass[64]{};

  // Піни
  PinsConfig pins{};

  // Енергетика
  EnergyConfig energy{};
  float price_kwh = energy.price_per_kwh; // аліас

  // VPN
  VPNConfig vpn{};

  char wg_private_key[60]{};   // base64(32) ~ 44 символи + запас
  char wg_peer_public[60]{};   // публічний ключ MikroTik
  char wg_endpoint[64]{};      // ip:port або dns:port
  char wg_allowed_ips[64]{};   // напр. "0.0.0.0/0" або "10.6.0.0/24"
  uint16_t wg_listen_port = 51820; // якщо потрібно
  // --- НОВЕ: калібровка піддуву (7 пар) ---
  // float calib_blower_t[7] = {40,50,60,70,80,90,100}; // °C
  // float calib_blower_p[7] = { 0,10,20,35,50,75,100}; // %
};

namespace CFG {
  extern AppConfig cfg;

  void begin();
  void load();
  bool save();

  bool savePins();
  bool saveEnergy();

  void resetToDefaults();
}
