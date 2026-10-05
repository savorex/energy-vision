-- CREATE TABLES:
--  measurements
--  meters

CREATE DATABASE IF NOT EXISTS `xamkenergy` CHARACTER
SET
  utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `xamkenergy`;

CREATE TABLE
  IF NOT EXISTS measurements (
    id BIGINT AUTO_INCREMENT,
    measurement_time DATETIME NOT NULL,
    meter_id VARCHAR(32) NOT NULL,
    kwh_import DECIMAL(12, 3),
    kwh_export DECIMAL(12, 3),
    kwh_production DECIMAL(12, 3),
    kwh_consumption DECIMAL(12, 3),
    measurement_type ENUM ('BN01', 'BN02', 'BN03') NOT NULL DEFAULT 'BN01',
    source_type ENUM ('ext', 'han', 'sim') NOT NULL DEFAULT 'ext',
    status_type ENUM ('ok', 'missing', 'estimated') NOT NULL DEFAULT 'ok',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id, measurement_time),
    UNIQUE KEY uniq_meter_time_source (meter_id, measurement_time, source_type, measurement_type),
    INDEX idx_meter_time (meter_id, measurement_time),
    INDEX idx_time (measurement_time)
  ) ENGINE = InnoDB
PARTITION BY
  RANGE (YEAR (measurement_time)) (
    PARTITION p_before_2024
    VALUES
      LESS THAN (2024),
      PARTITION p2024
    VALUES
      LESS THAN (2025),
      PARTITION p2025
    VALUES
      LESS THAN (2026),
      PARTITION p2026
    VALUES
      LESS THAN (2027),
      PARTITION p2027
    VALUES
      LESS THAN (2028),
      PARTITION p2028
    VALUES
      LESS THAN (2029),
      PARTITION p2029
    VALUES
      LESS THAN (2030),
      PARTITION p2030
    VALUES
      LESS THAN (2031),
      PARTITION p_future
    VALUES
      LESS THAN MAXVALUE
  );

CREATE TABLE IF NOT EXISTS
  meters (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    meter_id VARCHAR(32) NOT NULL,
    mp_ean VARCHAR(64) NULL,
    building VARCHAR(255),
    identifier VARCHAR(64) NOT NULL,
    state ENUM ('sim', 'active', 'retired') NOT NULL DEFAULT 'sim',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_updated TIMESTAMP NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uniq_meters_ean (mp_ean),
    UNIQUE KEY uniq_meters_identifier (identifier),
    INDEX idx_meters_meter_id (meter_id),
    CONSTRAINT chk_real_has_ean CHECK (
      state <> 'active'
      OR mp_ean IS NOT NULL
    )
  ) ENGINE = InnoDB;