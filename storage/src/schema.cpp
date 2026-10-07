#include "schema.h"

namespace ballistics::storage::detail {

// Schema history. Append new migrations; never edit a released one.
// All physical quantities are SI (m, kg, s, K, Pa, rad); BCs are the
// published lb/in^2 values.
const std::vector<Migration>& Migrations() {
    static const std::vector<Migration> migrations = {
        {1, R"sql(
CREATE TABLE app_setting (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE drag_curve (
    id     INTEGER PRIMARY KEY,
    name   TEXT NOT NULL,
    source TEXT NOT NULL DEFAULT '',
    notes  TEXT NOT NULL DEFAULT ''
);

CREATE TABLE drag_point (
    curve_id INTEGER NOT NULL REFERENCES drag_curve(id) ON DELETE CASCADE,
    mach     REAL NOT NULL CHECK (mach >= 0),
    cd       REAL NOT NULL CHECK (cd >= 0),
    PRIMARY KEY (curve_id, mach)
) WITHOUT ROWID;

CREATE TABLE bullet (
    id           INTEGER PRIMARY KEY,
    name         TEXT NOT NULL,
    manufacturer TEXT NOT NULL DEFAULT '',
    caliber      TEXT NOT NULL DEFAULT '',
    diameter_m   REAL NOT NULL CHECK (diameter_m > 0),
    mass_kg      REAL NOT NULL CHECK (mass_kg > 0),
    length_m     REAL NOT NULL DEFAULT 0 CHECK (length_m >= 0),
    drag_kind    TEXT NOT NULL CHECK (drag_kind IN ('bc', 'multi_bc', 'curve')),
    drag_table   TEXT NOT NULL DEFAULT 'G7',
    bc           REAL CHECK (bc IS NULL OR bc > 0),
    curve_id     INTEGER REFERENCES drag_curve(id) ON DELETE RESTRICT,
    form_factor  REAL NOT NULL DEFAULT 1 CHECK (form_factor > 0),
    source       TEXT NOT NULL DEFAULT '',
    notes        TEXT NOT NULL DEFAULT '',
    CHECK (drag_kind <> 'bc' OR bc IS NOT NULL),
    CHECK (drag_kind <> 'curve' OR curve_id IS NOT NULL)
);
CREATE INDEX bullet_by_caliber ON bullet(caliber, name);

CREATE TABLE bullet_bc_band (
    bullet_id    INTEGER NOT NULL REFERENCES bullet(id) ON DELETE CASCADE,
    velocity_mps REAL NOT NULL CHECK (velocity_mps > 0),
    bc           REAL NOT NULL CHECK (bc > 0),
    PRIMARY KEY (bullet_id, velocity_mps)
) WITHOUT ROWID;

CREATE TABLE cartridge (
    id                       INTEGER PRIMARY KEY,
    name                     TEXT NOT NULL,
    bullet_id                INTEGER NOT NULL REFERENCES bullet(id) ON DELETE RESTRICT,
    muzzle_velocity_mps      REAL NOT NULL CHECK (muzzle_velocity_mps > 0),
    reference_powder_temp_k  REAL NOT NULL DEFAULT 288.15,
    powder_sensitivity_per_k REAL NOT NULL DEFAULT 0,
    barrel_length_m          REAL NOT NULL DEFAULT 0,
    source                   TEXT NOT NULL DEFAULT '',
    notes                    TEXT NOT NULL DEFAULT ''
);
CREATE INDEX cartridge_by_bullet ON cartridge(bullet_id);

CREATE TABLE cartridge_velocity_point (
    cartridge_id  INTEGER NOT NULL REFERENCES cartridge(id) ON DELETE CASCADE,
    powder_temp_k REAL NOT NULL,
    velocity_mps  REAL NOT NULL CHECK (velocity_mps > 0),
    PRIMARY KEY (cartridge_id, powder_temp_k)
) WITHOUT ROWID;

CREATE TABLE rifle (
    id              INTEGER PRIMARY KEY,
    name            TEXT NOT NULL,
    caliber         TEXT NOT NULL DEFAULT '',
    barrel_length_m REAL NOT NULL DEFAULT 0,
    twist_m         REAL NOT NULL DEFAULT 0,
    sight_height_m  REAL NOT NULL DEFAULT 0,
    notes           TEXT NOT NULL DEFAULT ''
);

CREATE TABLE reticle (
    id                       INTEGER PRIMARY KEY,
    name                     TEXT NOT NULL,
    units                    TEXT NOT NULL CHECK (units IN ('mrad', 'moa')),
    focal_plane              TEXT NOT NULL DEFAULT 'ffp' CHECK (focal_plane IN ('ffp', 'sfp')),
    reference_magnification  REAL NOT NULL DEFAULT 0,
    definition               TEXT NOT NULL DEFAULT '',
    source                   TEXT NOT NULL DEFAULT ''
);

CREATE TABLE scope (
    id                  INTEGER PRIMARY KEY,
    name                TEXT NOT NULL,
    click_units         TEXT NOT NULL DEFAULT 'mrad'
                        CHECK (click_units IN ('mrad', 'moa', 'smoa', 'cm100m')),
    click_vertical_rad   REAL NOT NULL CHECK (click_vertical_rad > 0),
    click_horizontal_rad REAL NOT NULL CHECK (click_horizontal_rad > 0),
    reticle_id          INTEGER REFERENCES reticle(id) ON DELETE SET NULL,
    min_magnification   REAL NOT NULL DEFAULT 0,
    max_magnification   REAL NOT NULL DEFAULT 0,
    notes               TEXT NOT NULL DEFAULT ''
);

CREATE TABLE profile (
    id                     INTEGER PRIMARY KEY,
    name                   TEXT NOT NULL,
    rifle_id               INTEGER NOT NULL REFERENCES rifle(id) ON DELETE RESTRICT,
    scope_id               INTEGER REFERENCES scope(id) ON DELETE SET NULL,
    cartridge_id           INTEGER NOT NULL REFERENCES cartridge(id) ON DELETE RESTRICT,
    zero_range_m           REAL NOT NULL DEFAULT 100 CHECK (zero_range_m > 0),
    zero_offset_up_m       REAL NOT NULL DEFAULT 0,
    zero_offset_right_m    REAL NOT NULL DEFAULT 0,
    zero_altitude_m        REAL NOT NULL DEFAULT 0,
    zero_pressure_pa       REAL NOT NULL DEFAULT 101325,
    zero_temperature_k     REAL NOT NULL DEFAULT 288.15,
    zero_humidity          REAL NOT NULL DEFAULT 0,
    zero_powder_temp_k     REAL NOT NULL DEFAULT 288.15,
    velocity_scale         REAL NOT NULL DEFAULT 1 CHECK (velocity_scale > 0),
    drag_scale             REAL NOT NULL DEFAULT 1 CHECK (drag_scale > 0),
    created_at             TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    last_used_at           TEXT
);

CREATE TABLE conditions (
    id             INTEGER PRIMARY KEY,
    name           TEXT NOT NULL,
    altitude_m     REAL NOT NULL DEFAULT 0,
    pressure_pa    REAL NOT NULL DEFAULT 101325 CHECK (pressure_pa > 0),
    temperature_k  REAL NOT NULL DEFAULT 288.15 CHECK (temperature_k > 0),
    humidity       REAL NOT NULL DEFAULT 0 CHECK (humidity BETWEEN 0 AND 1),
    powder_temp_k  REAL,
    latitude_rad   REAL,
    azimuth_rad    REAL,
    look_angle_rad REAL NOT NULL DEFAULT 0,
    cant_rad       REAL NOT NULL DEFAULT 0,
    created_at     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

CREATE TABLE wind_zone (
    conditions_id INTEGER NOT NULL REFERENCES conditions(id) ON DELETE CASCADE,
    seq           INTEGER NOT NULL,
    until_range_m REAL NOT NULL,
    speed_mps     REAL NOT NULL CHECK (speed_mps >= 0),
    from_rad      REAL NOT NULL,
    vertical_mps  REAL NOT NULL DEFAULT 0,
    PRIMARY KEY (conditions_id, seq)
) WITHOUT ROWID;

CREATE TABLE dope_log (
    id                      INTEGER PRIMARY KEY,
    profile_id              INTEGER NOT NULL REFERENCES profile(id) ON DELETE CASCADE,
    shot_at                 TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    range_m                 REAL NOT NULL CHECK (range_m > 0),
    observed_elevation_rad  REAL NOT NULL,
    observed_windage_rad    REAL,
    predicted_elevation_rad REAL,
    predicted_windage_rad   REAL,
    altitude_m              REAL NOT NULL,
    pressure_pa             REAL NOT NULL,
    temperature_k           REAL NOT NULL,
    humidity                REAL NOT NULL,
    powder_temp_k           REAL,
    look_angle_rad          REAL NOT NULL DEFAULT 0,
    use_for_truing          INTEGER NOT NULL DEFAULT 1,
    notes                   TEXT NOT NULL DEFAULT ''
);
CREATE INDEX dope_log_by_profile ON dope_log(profile_id, range_m);
)sql"},
        // v2: focal plane per scope (the same reticle exists in FFP and SFP
        // scopes); for SFP the magnification its marks are true at.
        {2, R"sql(
ALTER TABLE scope ADD COLUMN focal_plane TEXT NOT NULL DEFAULT 'ffp'
    CHECK (focal_plane IN ('ffp', 'sfp'));
ALTER TABLE scope ADD COLUMN sfp_reference_magnification REAL NOT NULL DEFAULT 0;
)sql"},
        // v3: rifles and cartridges become independent. The scope and the
        // zero move from the profile to the rifle; a profile is now a
        // rifle + cartridge pair (point-of-impact shift, truing, shot log).
        // Until v2 every profile owned its rifle and cartridge, so each
        // rifle takes the values of its (only) profile. profile.scope_id
        // stays as an unused column: SQLite cannot drop a foreign key.
        {3, R"sql(
ALTER TABLE rifle ADD COLUMN scope_id INTEGER REFERENCES scope(id) ON DELETE SET NULL;
ALTER TABLE rifle ADD COLUMN zero_range_m REAL NOT NULL DEFAULT 100 CHECK (zero_range_m > 0);
ALTER TABLE rifle ADD COLUMN zero_altitude_m REAL NOT NULL DEFAULT 0;
ALTER TABLE rifle ADD COLUMN zero_pressure_pa REAL NOT NULL DEFAULT 101325;
ALTER TABLE rifle ADD COLUMN zero_temperature_k REAL NOT NULL DEFAULT 288.15;
ALTER TABLE rifle ADD COLUMN zero_humidity REAL NOT NULL DEFAULT 0;
ALTER TABLE rifle ADD COLUMN zero_powder_temp_k REAL NOT NULL DEFAULT 288.15;
ALTER TABLE cartridge ADD COLUMN caliber TEXT NOT NULL DEFAULT '';

UPDATE rifle SET
    scope_id           = p.scope_id,
    zero_range_m       = p.zero_range_m,
    zero_altitude_m    = p.zero_altitude_m,
    zero_pressure_pa   = p.zero_pressure_pa,
    zero_temperature_k = p.zero_temperature_k,
    zero_humidity      = p.zero_humidity,
    zero_powder_temp_k = p.zero_powder_temp_k
FROM (SELECT * FROM profile WHERE id IN (SELECT min(id) FROM profile GROUP BY rifle_id)) AS p
WHERE p.rifle_id = rifle.id;

UPDATE cartridge SET caliber = COALESCE(
    (SELECT r.caliber FROM profile p JOIN rifle r ON r.id = p.rifle_id
      WHERE p.cartridge_id = cartridge.id AND r.caliber <> '' ORDER BY p.id LIMIT 1),
    (SELECT b.caliber FROM bullet b WHERE b.id = cartridge.bullet_id),
    '');

UPDATE profile SET scope_id = NULL;
ALTER TABLE profile DROP COLUMN zero_range_m;
ALTER TABLE profile DROP COLUMN zero_altitude_m;
ALTER TABLE profile DROP COLUMN zero_pressure_pa;
ALTER TABLE profile DROP COLUMN zero_temperature_k;
ALTER TABLE profile DROP COLUMN zero_humidity;
ALTER TABLE profile DROP COLUMN zero_powder_temp_k;
CREATE UNIQUE INDEX profile_by_pair ON profile(rifle_id, cartridge_id);
CREATE INDEX profile_by_cartridge ON profile(cartridge_id);
)sql"},
        // v4: drag scale factor (DSF) table of a profile: the drag at a Mach
        // number is scaled by a factor interpolated between these points.
        {4, R"sql(
CREATE TABLE profile_dsf (
    profile_id INTEGER NOT NULL REFERENCES profile(id) ON DELETE CASCADE,
    mach       REAL NOT NULL CHECK (mach >= 0),
    factor     REAL NOT NULL CHECK (factor > 0),
    PRIMARY KEY (profile_id, mach)
) WITHOUT ROWID;
)sql"},
        // v5: a picture of a rifle or cartridge (JPEG/PNG), at most one each;
        // it goes with its owner.
        {5, R"sql(
CREATE TABLE photo (
    id       INTEGER PRIMARY KEY,
    kind     TEXT NOT NULL CHECK (kind IN ('rifle', 'cartridge')),
    owner_id INTEGER NOT NULL,
    image    BLOB NOT NULL,
    UNIQUE (kind, owner_id)
);
CREATE TRIGGER rifle_photo_gone AFTER DELETE ON rifle
BEGIN DELETE FROM photo WHERE kind = 'rifle' AND owner_id = OLD.id; END;
CREATE TRIGGER cartridge_photo_gone AFTER DELETE ON cartridge
BEGIN DELETE FROM photo WHERE kind = 'cartridge' AND owner_id = OLD.id; END;
)sql"},
    };
    return migrations;
}

}  // namespace ballistics::storage::detail
