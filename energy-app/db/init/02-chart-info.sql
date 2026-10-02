-- Chart metadata for the control panel
CREATE TABLE IF NOT EXISTS sankey_charts (
  id VARCHAR(64) PRIMARY KEY,
  title VARCHAR(128) NOT NULL,
  unit VARCHAR(16) NOT NULL DEFAULT '',
  description VARCHAR(255) NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

INSERT INTO sankey_charts (id, title, unit, description) VALUES
  ('energy', 'Energy Flow', 'kWh', 'Consumption, production and storage'),
  ('budget', 'Household Budget', '€', 'Monthly income vs. expenses')
ON DUPLICATE KEY UPDATE title = VALUES(title);
