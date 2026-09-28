-- [TABLES:]
--
-- CITIES
-- CAMPUSES
-- BUILDINGS
-- 
-- MEASUREMENTS
-- CALCULATIONS
--
CREATE TABLE cities (
	id INT AUTO_INCREMENT PRIMARY KEY,
	name VARCHAR(100) NOT NULL
);

INSERT INTO cities (name) VALUES
	('Mikkeli'),
	('Kouvola'),
	('Kotka'),
	('Savonlinna');

CREATE TABLE campuses (
	id INT AUTO_INCREMENT PRIMARY KEY,
	city_id INT NOT NULL,
	name VARCHAR(100) NOT NULL,
	FOREIGN KEY (city_id) REFERENCES cities (id)
);

INSERT INTO campuses (city_id, name) VALUES
	(1, 'Mikkelin kampus'),
	(2, 'Kouvolan kampus'),
	(3, 'Kotkan kampus'),
	(4, 'Savonlinnan kampus');

CREATE TABLE buildings (
    id INT AUTO_INCREMENT PRIMARY KEY,
    campus_id INT NOT NULL,
    name VARCHAR(100),
    FOREIGN KEY (campus_id) REFERENCES campuses (id)
);

CREATE TABLE measurements (
	id BIGINT AUTO_INCREMENT,
	meter_id INT NOT NULL,
	kwh DECIMAL(12,3) NOT NULL,
	measurement_time DATETIME NOT NULL,
	source ENUM('datahub', 'han', 'simulated') DEFAULT 'datahub',
	status ENUM('ok', 'missing', 'estimated') DEFAULT 'ok',
	PRIMARY KEY (id, measurement_time),
	UNIQUE KEY uniq_meter_time (meter_id, measurement_time),
	INDEX idx_meter_time (meter_id, measurement_time),
	INDEX idx_time (measurement_time)
) PARTITION BY RANGE (YEAR(measurement_time)) (
	PARTITION p2024 VALUES LESS THAN (2025),
	PARTITION p2025 VALUES LESS THAN (2026),
	PARTITION p2026 VALUES LESS THAN (2027),
	PARTITION p2027 VALUES LESS THAN (2028),
	PARTITION p2028 VALUES LESS THAN (2029),
	PARTITION p2029 VALUES LESS THAN (2030),
	PARTITION p2030 VALUES LESS THAN (2031),
	PARTITION p2031 VALUES LESS THAN (2032),
	PARTITION p_future VALUES LESS THAN MAXVALUE
);

CREATE TABLE calculations (
	id BIGINT AUTO_INCREMENT PRIMARY KEY,
	meter_id INT NOT NULL,
	period_start DATETIME NOT NULL,
	period_end DATETIME NOT NULL,
	total_kwh DECIMAL(14, 3) NOT NULL,
	avg_kw DECIMAL(10, 3),
	peak_kw DECIMAL(10, 3),
	calculation_type ENUM ('hourly', 'daily', 'monthly') NOT NULL,
	created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
	FOREIGN KEY (meter_id) REFERENCES meters (id),
	UNIQUE KEY uniq_calc (
		meter_id,
		period_start,
		period_end,
		calculation_type
	),
	INDEX idx_meter_period (meter_id, period_start, period_end)
);

-- TEMPORARY, REMOVE LATER: 
CREATE TABLE sankey_flows (
  id INT AUTO_INCREMENT PRIMARY KEY,
  chart_id VARCHAR(64) NOT NULL,
  node_from VARCHAR(128) NOT NULL,
  node_to VARCHAR(128) NOT NULL,
  value DOUBLE NOT NULL,
  color VARCHAR(16) NULL
);

-- TEMPORARY, REMOVE LATER: 
INSERT INTO sankey_flows (chart_id, node_from, node_to, value, color) VALUES
('energy', 'Solar', 'Grid', 40, '#facc15'),
('energy', 'Solar', 'Battery', 20, '#facc15'),
('energy', 'Wind', 'Grid', 35, '#38bdf8'),
('energy', 'Grid', 'Household', 55, '#4ade80'),
('energy', 'Grid', 'Industry', 20, '#f87171'),
('energy', 'Battery', 'Household', 15, '#a78bfa'),
('budget', 'Income', 'Housing', 900, '#38bdf8'),
('budget', 'Income', 'Food', 400, '#4ade80'),
('budget', 'Income', 'Savings', 300, '#facc15'),
('budget', 'Income', 'Other', 200, '#a78bfa');