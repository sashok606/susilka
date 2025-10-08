// src/web/WebUI.h
#pragma once
#include <WebServer.h>
#include <Arduino.h>

class WebUI {
public:
  void begin();
  void loop();

private:
  WebServer _srv{80};

  // helpers
  String _escape(const String& s);

  // root / state / manual / reboot
  void _handleRoot();
  void _handleState();
  void _handleStart();
  void _handleStop();
  void _handleManual();
  void _handleReboot();
  void _handleNotFound();

  // static assets
  void _mountStatic();
  String _staticAppCss();
  String _staticGaugeJs();
  String _staticAppJs();

  // settings + pins
  void _handleSettings();
  void _handleApply();
  void _handlePins();
  void _apiPinsGet();
  void _apiPinsSave();

  // ===== Профілі =====
  void _handleProfiles();      // сторінка зі списком
  void _handleProfile();       // сторінка редагування одного профілю
  void _handleApplyProf();     // <-- ВАЖЛИВО: це оголошення додати!

  // CRUD API для профілів
  void _apiProfiles();
  void _apiProfSelect();
  void _apiProfCreate();
  void _apiProfRename();
  void _apiProfDelete();

  // ===== Калібровки =====
  void _handleCalibration();       // піддув (температура -> %)
  void _apiCalibGet();
  void _apiCalibSave();

  void _handleCalibHumidity();     // вологість (ref/raw)
  void _apiCalibHumGet();
  void _apiCalibHumSave();
};
