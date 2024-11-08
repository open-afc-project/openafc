CREATE TABLE "afc_message" (
  "message_id" bigserial,
  "month_idx" smallint,
  "afc_server" serial,
  "rx_time" timestamptz,
  "tx_time" timestamptz,
  "rx_envelope_digest" uuid,
  "tx_envelope_digest" uuid,
  "dn_text_digest" uuid,
  PRIMARY KEY ("message_id", "month_idx")
);

CREATE TABLE "rx_envelope" (
  "rx_envelope_digest" uuid,
  "month_idx" smallint,
  "envelope_json" json,
  PRIMARY KEY ("rx_envelope_digest", "month_idx")
);

CREATE TABLE "tx_envelope" (
  "tx_envelope_digest" uuid,
  "month_idx" smallint,
  "envelope_json" json,
  PRIMARY KEY ("tx_envelope_digest", "month_idx")
);

CREATE TABLE "mtls_dn" (
  "dn_text_digest" uuid,
  "dn_json" jsonb,
  "dn_text" text,
  "month_idx" smallint,
  PRIMARY KEY ("dn_text_digest", "month_idx")
);

CREATE TABLE "request_response_in_message" (
  "message_id" bigint,
  "request_id" text,
  "month_idx" smallint,
  "request_response_digest" uuid,
  "expire_time" timestamptz,
  PRIMARY KEY ("message_id", "request_id", "month_idx")
);

CREATE TABLE "request_response" (
  "request_response_digest" uuid,
  "month_idx" smallint,
  "afc_config_text_digest" uuid,
  "customer_id" integer,
  "uls_data_version_id" integer,
  "geo_data_version_id" integer,
  "request_json_digest" uuid,
  "response_json_digest" uuid,
  "device_descriptor_digest" uuid,
  "location_digest" uuid,
  "response_code" int,
  "response_description" text,
  "response_data" text,
  PRIMARY KEY ("request_response_digest", "month_idx")
);

CREATE TABLE "device_descriptor" (
  "device_descriptor_digest" uuid,
  "month_idx" smallint,
  "serial_number" text,
  "certifications_digest" uuid,
  PRIMARY KEY ("device_descriptor_digest", "month_idx")
);

CREATE TABLE "certification" (
  "certifications_digest" uuid,
  "certification_index" smallint,
  "month_idx" smallint,
  "ruleset_id" text,
  "certification_id" text,
  PRIMARY KEY ("certifications_digest", "certification_index", "month_idx")
);

CREATE TABLE "compressed_json" (
  "compressed_json_digest" uuid,
  "month_idx" smallint,
  "compressed_json_data" bytea,
  PRIMARY KEY ("compressed_json_digest", "month_idx")
);

CREATE TABLE "customer" (
  "customer_id" serial,
  "month_idx" smallint,
  "customer_name" text,
  PRIMARY KEY ("customer_id", "month_idx")
);

CREATE TABLE "location" (
  "location_digest" uuid,
  "month_idx" smallint,
  "location_wgs84" geography(POINT,4326),
  "location_uncertainty_m" real,
  "location_type" text,
  "deployment_type" int,
  "height_m" real,
  "height_uncertainty_m" real,
  "height_type" text,
  PRIMARY KEY ("location_digest", "month_idx")
);

CREATE TABLE "afc_config" (
  "afc_config_text_digest" uuid,
  "month_idx" smallint,
  "afc_config_text" text,
  "afc_config_json" json,
  PRIMARY KEY ("afc_config_text_digest", "month_idx")
);

CREATE TABLE "geo_data_version" (
  "geo_data_version_id" serial,
  "month_idx" smallint,
  "geo_data_version" text,
  PRIMARY KEY ("geo_data_version_id", "month_idx")
);

CREATE TABLE "uls_data_version" (
  "uls_data_version_id" serial,
  "month_idx" smallint,
  "uls_data_version" text,
  PRIMARY KEY ("uls_data_version_id", "month_idx")
);

CREATE TABLE "max_psd" (
  "request_response_digest" uuid,
  "month_idx" smallint,
  "low_frequency_mhz" smallint,
  "high_frequency_mhz" smallint,
  "max_psd_dbm_mhz" real,
  PRIMARY KEY ("request_response_digest", "month_idx", "low_frequency_mhz", "high_frequency_mhz")
);

CREATE TABLE "max_eirp" (
  "request_response_digest" uuid,
  "month_idx" smallint,
  "op_class" smallint,
  "channel" smallint,
  "max_eirp_dbm" real,
  PRIMARY KEY ("request_response_digest", "month_idx", "op_class", "channel")
);

CREATE TABLE "afc_server" (
  "afc_server_id" serial,
  "month_idx" smallint,
  "afc_server_name" text,
  PRIMARY KEY ("afc_server_id", "month_idx")
);

CREATE TABLE "decode_error" (
  "id" bigserial PRIMARY KEY,
  "time" timestamptz,
  "msg" text,
  "code_line" integer,
  "data" text,
  "month_idx" smallint
);

CREATE INDEX ON "afc_message" ("rx_time");

CREATE INDEX ON "afc_message" ("tx_time");

CREATE INDEX ON "rx_envelope" USING HASH ("rx_envelope_digest");

CREATE INDEX ON "tx_envelope" USING HASH ("tx_envelope_digest");

CREATE INDEX ON "mtls_dn" USING HASH ("dn_text_digest");

CREATE INDEX ON "request_response_in_message" ("request_id");

CREATE INDEX ON "request_response_in_message" ("request_response_digest");

CREATE INDEX ON "request_response_in_message" ("expire_time");

CREATE INDEX ON "request_response" USING HASH ("request_response_digest");

CREATE INDEX ON "request_response" ("afc_config_text_digest");

CREATE INDEX ON "request_response" ("customer_id");

CREATE INDEX ON "request_response" ("device_descriptor_digest");

CREATE INDEX ON "request_response" ("location_digest");

CREATE INDEX ON "request_response" ("response_code");

CREATE INDEX ON "request_response" ("response_description");

CREATE INDEX ON "request_response" ("response_data");

CREATE INDEX ON "device_descriptor" USING HASH ("device_descriptor_digest");

CREATE INDEX ON "device_descriptor" ("serial_number");

CREATE INDEX ON "device_descriptor" ("certifications_digest");

CREATE INDEX ON "certification" USING HASH ("certifications_digest");

CREATE INDEX ON "certification" ("ruleset_id");

CREATE INDEX ON "certification" ("certification_id");

CREATE INDEX ON "compressed_json" USING HASH ("compressed_json_digest");

CREATE INDEX ON "customer" ("customer_name");

CREATE INDEX ON "location" USING HASH ("location_digest");

CREATE INDEX ON "location" ("location_wgs84");

CREATE INDEX ON "location" ("location_type");

CREATE INDEX ON "location" ("height_m");

CREATE INDEX ON "location" ("height_type");

CREATE INDEX ON "afc_config" USING HASH ("afc_config_text_digest");

CREATE UNIQUE INDEX ON "geo_data_version" ("geo_data_version", "month_idx");

CREATE INDEX ON "geo_data_version" ("geo_data_version");

CREATE UNIQUE INDEX ON "uls_data_version" ("uls_data_version", "month_idx");

CREATE INDEX ON "uls_data_version" ("uls_data_version");

CREATE INDEX ON "max_psd" USING HASH ("request_response_digest");

CREATE INDEX ON "max_psd" ("low_frequency_mhz");

CREATE INDEX ON "max_psd" ("high_frequency_mhz");

CREATE INDEX ON "max_psd" ("max_psd_dbm_mhz");

CREATE INDEX ON "max_eirp" USING HASH ("request_response_digest");

CREATE INDEX ON "max_eirp" ("op_class");

CREATE INDEX ON "max_eirp" ("channel");

CREATE INDEX ON "max_eirp" ("max_eirp_dbm");

CREATE UNIQUE INDEX ON "afc_server" ("afc_server_name", "month_idx");

CREATE INDEX ON "afc_server" ("afc_server_name");

COMMENT ON TABLE "afc_message" IS 'AFC Request/Response message pair (contain individual requests/responses)';

COMMENT ON COLUMN "afc_message"."rx_envelope_digest" IS 'Envelope of AFC Request message';

COMMENT ON COLUMN "afc_message"."tx_envelope_digest" IS 'Envelope of AFC Response message';

COMMENT ON COLUMN "afc_message"."dn_text_digest" IS 'mTLS DN digest';

COMMENT ON TABLE "rx_envelope" IS 'Envelope (constant part) of AFC Request Message';

COMMENT ON COLUMN "rx_envelope"."rx_envelope_digest" IS 'MD5 of envelope_json field in UTF8 encoding';

COMMENT ON COLUMN "rx_envelope"."envelope_json" IS 'AFC Request JSON with empty availableSpectrumInquiryRequests field';

COMMENT ON TABLE "tx_envelope" IS 'Envelope (constant part) of AFC Response Message';

COMMENT ON COLUMN "tx_envelope"."tx_envelope_digest" IS 'MD5 of envelope_json field in UTF8 encoding';

COMMENT ON COLUMN "tx_envelope"."envelope_json" IS 'AFC Response JSON with empty availableSpectrumInquiryRequests field';

COMMENT ON TABLE "mtls_dn" IS 'mTLS certificate DN components';

COMMENT ON COLUMN "mtls_dn"."dn_text_digest" IS 'Digest computed over string representation of DN';

COMMENT ON COLUMN "mtls_dn"."dn_json" IS 'Components of certificate distinguished name';

COMMENT ON COLUMN "mtls_dn"."dn_text" IS 'Certificate distinquished name as string';

COMMENT ON TABLE "request_response_in_message" IS 'Associative table for relatonship between AFC Request/Response messages and individual requests/responses. Also encapsulates variable part of requests/responses';

COMMENT ON COLUMN "request_response_in_message"."message_id" IS 'AFC request/response message pair this request/response belongs';

COMMENT ON COLUMN "request_response_in_message"."request_id" IS 'ID of request/response within message';

COMMENT ON COLUMN "request_response_in_message"."request_response_digest" IS 'Reference to potentially constant part of request/response';

COMMENT ON COLUMN "request_response_in_message"."expire_time" IS 'Response expiration time';

COMMENT ON TABLE "request_response" IS 'Potentiially constant part of request/response';

COMMENT ON COLUMN "request_response"."request_response_digest" IS 'MD5 computed over request/response with requestId and availabilityExpireTime fields set to empty';

COMMENT ON COLUMN "request_response"."afc_config_text_digest" IS 'MD5 over used AFC Config text represnetation';

COMMENT ON COLUMN "request_response"."customer_id" IS 'AP vendor';

COMMENT ON COLUMN "request_response"."uls_data_version_id" IS 'Version of used ULS data';

COMMENT ON COLUMN "request_response"."geo_data_version_id" IS 'Version of used geospatial data';

COMMENT ON COLUMN "request_response"."request_json_digest" IS 'MD5 of request JSON with empty requestId';

COMMENT ON COLUMN "request_response"."response_json_digest" IS 'MD5 of resaponse JSON with empty requesatId and availabilityExpireTime';

COMMENT ON COLUMN "request_response"."device_descriptor_digest" IS 'MD5 of device descriptor (AP) related part of request JSON';

COMMENT ON COLUMN "request_response"."location_digest" IS 'MD5 of location-related part of request JSON';

COMMENT ON COLUMN "request_response"."response_description" IS 'Optional response code short description. Null for success';

COMMENT ON COLUMN "request_response"."response_data" IS 'Optional supplemental failure information. Optional comma-separated list of missing/invalid/unexpected parameters, etc.';

COMMENT ON TABLE "device_descriptor" IS 'Information about device (e.g. AP)';

COMMENT ON COLUMN "device_descriptor"."device_descriptor_digest" IS 'MD5 over parts of requesat JSON pertinent to AP';

COMMENT ON COLUMN "device_descriptor"."serial_number" IS 'AP serial number';

COMMENT ON COLUMN "device_descriptor"."certifications_digest" IS 'Device certifications';

COMMENT ON TABLE "certification" IS 'Element of certifications list';

COMMENT ON COLUMN "certification"."certifications_digest" IS 'MD5 of certification list json';

COMMENT ON COLUMN "certification"."certification_index" IS 'Index in certification list';

COMMENT ON COLUMN "certification"."ruleset_id" IS 'Name of rules for which AP certified (equivalent of region)';

COMMENT ON COLUMN "certification"."certification_id" IS 'ID of certification (equivalent of manufacturer)';

COMMENT ON TABLE "compressed_json" IS 'Compressed body of request or response';

COMMENT ON COLUMN "compressed_json"."compressed_json_digest" IS 'MD5 hash of compressed data';

COMMENT ON COLUMN "compressed_json"."compressed_json_data" IS 'Compressed data';

COMMENT ON TABLE "customer" IS 'Customer aka vendor aka user';

COMMENT ON COLUMN "customer"."customer_name" IS 'Its name';

COMMENT ON TABLE "location" IS 'AP location';

COMMENT ON COLUMN "location"."location_digest" IS 'MD5 computed over location part of request JSON';

COMMENT ON COLUMN "location"."location_wgs84" IS 'AP area center (WGS84 coordinates)';

COMMENT ON COLUMN "location"."location_uncertainty_m" IS 'Radius of AP uncertainty area in meters';

COMMENT ON COLUMN "location"."location_type" IS 'Ellipse/LinearPolygon/RadialPolygon';

COMMENT ON COLUMN "location"."deployment_type" IS '0/1/2 for unknown/indoor/outdoor';

COMMENT ON COLUMN "location"."height_m" IS 'AP elevation in meters';

COMMENT ON COLUMN "location"."height_uncertainty_m" IS 'Elevation uncertainty in meters';

COMMENT ON COLUMN "location"."height_type" IS 'Elevation type';

COMMENT ON TABLE "afc_config" IS 'AFC Config';

COMMENT ON COLUMN "afc_config"."afc_config_text_digest" IS 'MD5 computed over text representation';

COMMENT ON COLUMN "afc_config"."afc_config_text" IS 'Text representation of AFC Config';

COMMENT ON COLUMN "afc_config"."afc_config_json" IS 'JSON representation of AFC Config';

COMMENT ON TABLE "geo_data_version" IS 'Version of geospatial data';

COMMENT ON TABLE "uls_data_version" IS 'Version of ULS data';

COMMENT ON TABLE "max_psd" IS 'PSD result';

COMMENT ON COLUMN "max_psd"."request_response_digest" IS 'Request this result belongs to';

COMMENT ON TABLE "max_eirp" IS 'EIRP result';

COMMENT ON COLUMN "max_eirp"."request_response_digest" IS 'Request this result belongs to';

ALTER TABLE "afc_message" ADD CONSTRAINT "afc_message_afc_server_ref" FOREIGN KEY ("afc_server", "month_idx") REFERENCES "afc_server" ("afc_server_id", "month_idx");

ALTER TABLE "afc_message" ADD CONSTRAINT "afc_message_rx_envelope_digest_ref" FOREIGN KEY ("rx_envelope_digest", "month_idx") REFERENCES "rx_envelope" ("rx_envelope_digest", "month_idx");

ALTER TABLE "afc_message" ADD CONSTRAINT "afc_message_tx_envelope_digest_ref" FOREIGN KEY ("tx_envelope_digest", "month_idx") REFERENCES "tx_envelope" ("tx_envelope_digest", "month_idx");

ALTER TABLE "afc_message" ADD CONSTRAINT "afc_message_dn_text_digest_ref" FOREIGN KEY ("dn_text_digest", "month_idx") REFERENCES "mtls_dn" ("dn_text_digest", "month_idx");

ALTER TABLE "request_response_in_message" ADD CONSTRAINT "request_response_in_message_message_id_ref" FOREIGN KEY ("message_id", "month_idx") REFERENCES "afc_message" ("message_id", "month_idx");

ALTER TABLE "request_response_in_message" ADD CONSTRAINT "request_response_in_message_request_response_digest_ref" FOREIGN KEY ("request_response_digest", "month_idx") REFERENCES "request_response" ("request_response_digest", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_afc_config_text_digest_ref" FOREIGN KEY ("afc_config_text_digest", "month_idx") REFERENCES "afc_config" ("afc_config_text_digest", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_customer_id_ref" FOREIGN KEY ("customer_id", "month_idx") REFERENCES "customer" ("customer_id", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_uls_data_version_id_ref" FOREIGN KEY ("uls_data_version_id", "month_idx") REFERENCES "uls_data_version" ("uls_data_version_id", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_geo_data_version_id_ref" FOREIGN KEY ("geo_data_version_id", "month_idx") REFERENCES "geo_data_version" ("geo_data_version_id", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_request_json_digest_ref" FOREIGN KEY ("request_json_digest", "month_idx") REFERENCES "compressed_json" ("compressed_json_digest", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_response_json_digest_ref" FOREIGN KEY ("response_json_digest", "month_idx") REFERENCES "compressed_json" ("compressed_json_digest", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_device_descriptor_digest_ref" FOREIGN KEY ("device_descriptor_digest", "month_idx") REFERENCES "device_descriptor" ("device_descriptor_digest", "month_idx");

ALTER TABLE "request_response" ADD CONSTRAINT "request_response_location_digest_ref" FOREIGN KEY ("location_digest", "month_idx") REFERENCES "location" ("location_digest", "month_idx");

CREATE TABLE "device_descriptor_certification" (
  "device_descriptor_certifications_digest" uuid,
  "device_descriptor_month_idx" smallint,
  "certification_certifications_digest" uuid,
  "certification_month_idx" smallint,
  PRIMARY KEY ("device_descriptor_certifications_digest", "device_descriptor_month_idx", "certification_certifications_digest", "certification_month_idx")
);

ALTER TABLE "device_descriptor_certification" ADD FOREIGN KEY ("device_descriptor_certifications_digest", "device_descriptor_month_idx") REFERENCES "device_descriptor" ("certifications_digest", "month_idx");

ALTER TABLE "device_descriptor_certification" ADD FOREIGN KEY ("certification_certifications_digest", "certification_month_idx") REFERENCES "certification" ("certifications_digest", "month_idx");


ALTER TABLE "max_psd" ADD CONSTRAINT "max_psd_request_response_digest_ref" FOREIGN KEY ("request_response_digest", "month_idx") REFERENCES "request_response" ("request_response_digest", "month_idx");

ALTER TABLE "max_eirp" ADD CONSTRAINT "max_eirp_request_response_digest_ref" FOREIGN KEY ("request_response_digest", "month_idx") REFERENCES "request_response" ("request_response_digest", "month_idx");
