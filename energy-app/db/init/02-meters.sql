-- ---------------------------------------------------------------------
-- Demo data for meters and measurements
-- ---------------------------------------------------------------------
-- Real meter
INSERT INTO meters (meter_id, mp_ean, state, building, identifier) VALUES
  ('1120000', '7359990001234567', 'active', 'RAK05',  'PK10');

-- Simulated meters: NULL EAN, systematic identifiers
INSERT INTO meters (meter_id, mp_ean, building, identifier) VALUES
  ('1000000', NULL, 'RAK01', 'sim-0001'),
  ('1100000', NULL, 'RAK01', 'sim-0002'),
  ('1110000', NULL, 'RAK01', 'sim-0003'),
  ('1110100', NULL, 'RAK02', 'sim-0004'),
  ('1110200', NULL, 'RAK02', 'sim-0005'),
  ('1110300', NULL, 'RAK02', 'sim-0006'),
  ('1110400', NULL, 'RAK03', 'sim-0007'),
  ('1110500', NULL, 'RAK03', 'sim-0008'),
  ('1110600', NULL, 'RAK03', 'sim-0009'),
  ('1110700', NULL, 'RAK04', 'sim-0010'),
  ('1110800', NULL, 'RAK04', 'sim-0011'),
  ('1110900', NULL, 'RAK04', 'sim-0012'),
  ('1120100', NULL, 'RAK05', 'sim-0013'),
  ('1120200', NULL, 'RAK05', 'sim-0014'),
  ('1120201', NULL, 'RAK05', 'sim-0015'),
  ('1120202', NULL, 'RAK06', 'sim-0016'),
  ('1120203', NULL, 'RAK06', 'sim-0017'),
  ('1120204', NULL, 'RAK06', 'sim-0018'),
  ('1120205', NULL, 'RAK07', 'sim-0019'),
  ('1120206', NULL, 'RAK07', 'sim-0020'),
  ('1120207', NULL, 'RAK07', 'sim-0021'),
  ('1120208', NULL, 'RAK08', 'sim-0022'),
  ('1120209', NULL, 'RAK08', 'sim-0023'),
  ('1120210', NULL, 'RAK08', 'sim-0024'),
  ('1120211', NULL, 'RAK09', 'sim-0025'),
  ('1120300', NULL, 'RAK09', 'sim-0026'),
  ('1120400', NULL, 'RAK09', 'sim-0027'),
  ('1120500', NULL, 'RAK10', 'sim-0028'),
  ('1120600', NULL, 'RAK10', 'sim-0029'),
  ('1200000', NULL, 'RAK10', 'sim-0030'),
  ('1210000', NULL, 'RAK10', 'sim-0031'),
  ('1220000', NULL, 'RAK10', 'sim-0032');
