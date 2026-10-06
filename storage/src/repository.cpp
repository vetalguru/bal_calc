#include <ballistics/storage/repository.h>

#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <sqlite_manager/statement.h>
#include <sqlite_manager/transaction.h>

namespace ballistics::storage {

namespace {

using sqlite_manager::Error;
using sqlite_manager::ErrorCode;
using sqlite_manager::Ok;
using sqlite_manager::Statement;
using sqlite_manager::Transaction;
using sqlite_manager::ValueType;

// A column value as stored by SQLite.
using Value = std::variant<std::monostate, std::int64_t, double, std::string>;

double AsDouble(const Value& v) {
    if (const auto* d = std::get_if<double>(&v)) {
        return *d;
    }
    if (const auto* i = std::get_if<std::int64_t>(&v)) {
        return static_cast<double>(*i);
    }
    return 0.0;
}

std::int64_t AsInt(const Value& v) {
    if (const auto* i = std::get_if<std::int64_t>(&v)) {
        return *i;
    }
    if (const auto* d = std::get_if<double>(&v)) {
        return static_cast<std::int64_t>(*d);
    }
    return 0;
}

std::string AsText(const Value& v) {
    if (const auto* s = std::get_if<std::string>(&v)) {
        return *s;
    }
    return {};
}

bool IsNull(const Value& v) { return std::holds_alternative<std::monostate>(v); }

template <typename T>
struct Column {
    const char* name;
    std::function<Value(const T&)> get;
    std::function<void(T&, const Value&)> set;
    bool insert = true; // false: filled by the database (defaults)
};

// Column bound to a data member; the member type picks the conversion.
template <typename T, typename M>
Column<T> Col(const char* name, M T::*member) {
    Column<T> c{name, nullptr, nullptr};
    c.get = [member](const T& r) -> Value {
        const M& m = r.*member;
        if constexpr (std::is_same_v<M, double>) {
            return m;
        } else if constexpr (std::is_same_v<M, bool>) {
            return std::int64_t{m ? 1 : 0};
        } else if constexpr (std::is_integral_v<M>) {
            return static_cast<std::int64_t>(m);
        } else if constexpr (std::is_same_v<M, std::string>) {
            return m;
        } else if constexpr (std::is_same_v<M, std::optional<double>>) {
            return m ? Value{*m} : Value{};
        } else if constexpr (std::is_same_v<M, std::optional<std::int64_t>>) {
            return m ? Value{*m} : Value{};
        } else if constexpr (std::is_same_v<M, std::optional<std::string>>) {
            return m ? Value{*m} : Value{};
        } else {
            static_assert(sizeof(M) == 0, "unsupported column type");
        }
    };
    c.set = [member](T& r, const Value& v) {
        M& m = r.*member;
        if constexpr (std::is_same_v<M, double>) {
            m = AsDouble(v);
        } else if constexpr (std::is_same_v<M, bool>) {
            m = AsInt(v) != 0;
        } else if constexpr (std::is_integral_v<M>) {
            m = static_cast<M>(AsInt(v));
        } else if constexpr (std::is_same_v<M, std::string>) {
            m = AsText(v);
        } else if constexpr (std::is_same_v<M, std::optional<double>>) {
            m = IsNull(v) ? std::nullopt : std::optional<double>(AsDouble(v));
        } else if constexpr (std::is_same_v<M, std::optional<std::int64_t>>) {
            m = IsNull(v) ? std::nullopt : std::optional<std::int64_t>(AsInt(v));
        } else if constexpr (std::is_same_v<M, std::optional<std::string>>) {
            m = IsNull(v) ? std::nullopt : std::optional<std::string>(AsText(v));
        }
    };
    return c;
}

// Column bound to a member of a nested struct (e.g. atmosphere fields).
template <typename T, typename S>
Column<T> Nested(const char* name, S T::*outer, double S::*inner) {
    return {name, [outer, inner](const T& r) -> Value { return (r.*outer).*inner; },
            [outer, inner](T& r, const Value& v) { (r.*outer).*inner = AsDouble(v); }};
}

// Text column the database fills with its default when left empty.
template <typename T>
Column<T> DbDefaultText(const char* name, std::string T::*member) {
    Column<T> c = Col(name, member);
    c.insert = false;
    return c;
}

template <typename T>
struct Table {
    const char* name;
    const char* order_by;
    std::vector<Column<T>> columns;
    // Replace / load the record's child rows; may be empty.
    std::function<Status(Database&, const T&)> save_children;
    std::function<Status(Database&, T&)> load_children;
};

Status Bind(Statement& st, const std::string& param, const Value& v) {
    if (const auto* i = std::get_if<std::int64_t>(&v)) {
        return st.BindInt64(param, *i);
    }
    if (const auto* d = std::get_if<double>(&v)) {
        return st.BindDouble(param, *d);
    }
    if (const auto* s = std::get_if<std::string>(&v)) {
        return st.BindText(param, *s);
    }
    return st.BindNull(param);
}

Value Read(const Statement& st, int col) {
    switch (st.ColumnType(col)) {
        case ValueType::kInteger: return st.ColumnInt64(col);
        case ValueType::kFloat: return st.ColumnDouble(col);
        case ValueType::kText: return st.ColumnText(col);
        case ValueType::kBlob:
        case ValueType::kNull: break;
    }
    return {};
}

// Runs a statement to completion with positional parameters.
Status Exec(Database& db, const std::string& sql, const std::vector<Value>& params) {
    auto st = Statement::Prepare(db.connection(), sql);
    if (!st) {
        return st.error();
    }
    for (std::size_t i = 0; i < params.size(); ++i) {
        const Value& v = params[i];
        const int idx = static_cast<int>(i) + 1;
        Status s = Ok();
        if (const auto* n = std::get_if<std::int64_t>(&v)) {
            s = st.value().BindInt64(idx, *n);
        } else if (const auto* d = std::get_if<double>(&v)) {
            s = st.value().BindDouble(idx, *d);
        } else if (const auto* t = std::get_if<std::string>(&v)) {
            s = st.value().BindText(idx, *t);
        } else {
            s = st.value().BindNull(idx);
        }
        if (!s) {
            return s;
        }
    }
    while (true) {
        auto step = st.value().Step();
        if (!step) {
            return step.error();
        }
        if (step.value() == Statement::StepResult::kDone) {
            return Ok();
        }
    }
}

// Reads all rows of a query with one int64 parameter.
template <typename Row>
Result<std::vector<Row>> Rows(Database& db, const std::string& sql, std::int64_t param,
                              const std::function<Row(const Statement&)>& read) {
    auto st = Statement::Prepare(db.connection(), sql);
    if (!st) {
        return st.error();
    }
    if (Status s = st.value().BindInt64(1, param); !s) {
        return s.error();
    }
    std::vector<Row> out;
    while (true) {
        auto step = st.value().Step();
        if (!step) {
            return step.error();
        }
        if (step.value() == Statement::StepResult::kDone) {
            return out;
        }
        out.push_back(read(st.value()));
    }
}

// --- Table descriptions ------------------------------------------------

template <typename T>
const Table<T>& TableOf();

template <>
const Table<DragCurveRecord>& TableOf() {
    using R = DragCurveRecord;
    static const Table<R> t{
        "drag_curve",
        "name, id",
        {Col("name", &R::name), Col("source", &R::source), Col("notes", &R::notes)},
        [](Database& db, const R& r) -> Status {
            if (Status s = Exec(db, "DELETE FROM drag_point WHERE curve_id = ?", {r.id}); !s) {
                return s;
            }
            for (const DragPoint& p : r.points) {
                if (Status s = Exec(db, "INSERT INTO drag_point (curve_id, mach, cd) VALUES (?, ?, ?)",
                                    {r.id, p.mach, p.cd});
                    !s) {
                    return s;
                }
            }
            return Ok();
        },
        [](Database& db, R& r) -> Status {
            auto rows = Rows<DragPoint>(
                db, "SELECT mach, cd FROM drag_point WHERE curve_id = ? ORDER BY mach", r.id,
                [](const Statement& st) { return DragPoint{st.ColumnDouble(0), st.ColumnDouble(1)}; });
            if (!rows) {
                return rows.error();
            }
            r.points = std::move(rows).value();
            return Ok();
        }};
    return t;
}

template <>
const Table<BulletRecord>& TableOf() {
    using R = BulletRecord;
    static const Table<R> t{
        "bullet",
        "caliber, name, id",
        {Col("name", &R::name), Col("manufacturer", &R::manufacturer), Col("caliber", &R::caliber),
         Col("diameter_m", &R::diameter_m), Col("mass_kg", &R::mass_kg),
         Col("length_m", &R::length_m), Col("drag_kind", &R::drag_kind),
         Col("drag_table", &R::drag_table), Col("bc", &R::bc), Col("curve_id", &R::curve_id),
         Col("form_factor", &R::form_factor), Col("source", &R::source), Col("notes", &R::notes)},
        [](Database& db, const R& r) -> Status {
            if (Status s = Exec(db, "DELETE FROM bullet_bc_band WHERE bullet_id = ?", {r.id}); !s) {
                return s;
            }
            for (const BcPoint& p : r.bc_bands) {
                if (Status s = Exec(db,
                                    "INSERT INTO bullet_bc_band (bullet_id, velocity_mps, bc) "
                                    "VALUES (?, ?, ?)",
                                    {r.id, p.velocity_mps, p.bc_lb_in2});
                    !s) {
                    return s;
                }
            }
            return Ok();
        },
        [](Database& db, R& r) -> Status {
            auto rows = Rows<BcPoint>(
                db,
                "SELECT velocity_mps, bc FROM bullet_bc_band WHERE bullet_id = ? "
                "ORDER BY velocity_mps DESC",
                r.id, [](const Statement& st) { return BcPoint{st.ColumnDouble(0), st.ColumnDouble(1)}; });
            if (!rows) {
                return rows.error();
            }
            r.bc_bands = std::move(rows).value();
            return Ok();
        }};
    return t;
}

template <>
const Table<CartridgeRecord>& TableOf() {
    using R = CartridgeRecord;
    using Point = std::pair<double, double>;
    static const Table<R> t{
        "cartridge",
        "name, id",
        {Col("name", &R::name), Col("caliber", &R::caliber), Col("bullet_id", &R::bullet_id),
         Col("muzzle_velocity_mps", &R::muzzle_velocity_mps),
         Col("reference_powder_temp_k", &R::reference_powder_temp_k),
         Col("powder_sensitivity_per_k", &R::powder_sensitivity_per_k),
         Col("barrel_length_m", &R::barrel_length_m), Col("source", &R::source),
         Col("notes", &R::notes)},
        [](Database& db, const R& r) -> Status {
            if (Status s =
                    Exec(db, "DELETE FROM cartridge_velocity_point WHERE cartridge_id = ?", {r.id});
                !s) {
                return s;
            }
            for (const auto& [temp, v] : r.velocity_points) {
                if (Status s = Exec(db,
                                    "INSERT INTO cartridge_velocity_point "
                                    "(cartridge_id, powder_temp_k, velocity_mps) VALUES (?, ?, ?)",
                                    {r.id, temp, v});
                    !s) {
                    return s;
                }
            }
            return Ok();
        },
        [](Database& db, R& r) -> Status {
            auto rows = Rows<Point>(
                db,
                "SELECT powder_temp_k, velocity_mps FROM cartridge_velocity_point "
                "WHERE cartridge_id = ? ORDER BY powder_temp_k",
                r.id, [](const Statement& st) { return Point{st.ColumnDouble(0), st.ColumnDouble(1)}; });
            if (!rows) {
                return rows.error();
            }
            r.velocity_points = std::move(rows).value();
            return Ok();
        }};
    return t;
}

template <>
const Table<RifleRecord>& TableOf() {
    using R = RifleRecord;
    static const Table<R> t{"rifle",
                            "name, id",
                            {Col("name", &R::name), Col("caliber", &R::caliber),
                             Col("barrel_length_m", &R::barrel_length_m),
                             Col("twist_m", &R::twist_m), Col("sight_height_m", &R::sight_height_m),
                             Col("notes", &R::notes), Col("scope_id", &R::scope_id),
                             Col("zero_range_m", &R::zero_range_m),
                             Nested("zero_altitude_m", &R::zero_atmosphere, &Atmosphere::altitude_m),
                             Nested("zero_pressure_pa", &R::zero_atmosphere, &Atmosphere::pressure_pa),
                             Nested("zero_temperature_k", &R::zero_atmosphere,
                                    &Atmosphere::temperature_k),
                             Nested("zero_humidity", &R::zero_atmosphere, &Atmosphere::humidity),
                             Col("zero_powder_temp_k", &R::zero_powder_temp_k)},
                            nullptr,
                            nullptr};
    return t;
}

template <>
const Table<ReticleRecord>& TableOf() {
    using R = ReticleRecord;
    static const Table<R> t{"reticle",
                            "name, id",
                            {Col("name", &R::name), Col("units", &R::units),
                             Col("focal_plane", &R::focal_plane),
                             Col("reference_magnification", &R::reference_magnification),
                             Col("definition", &R::definition), Col("source", &R::source)},
                            nullptr,
                            nullptr};
    return t;
}

template <>
const Table<ScopeRecord>& TableOf() {
    using R = ScopeRecord;
    static const Table<R> t{"scope",
                            "name, id",
                            {Col("name", &R::name), Col("click_units", &R::click_units),
                             Col("click_vertical_rad", &R::click_vertical_rad),
                             Col("click_horizontal_rad", &R::click_horizontal_rad),
                             Col("reticle_id", &R::reticle_id),
                             Col("min_magnification", &R::min_magnification),
                             Col("max_magnification", &R::max_magnification),
                             Col("notes", &R::notes), Col("focal_plane", &R::focal_plane),
                             Col("sfp_reference_magnification", &R::sfp_reference_magnification)},
                            nullptr,
                            nullptr};
    return t;
}

template <>
const Table<ProfileRecord>& TableOf() {
    using R = ProfileRecord;
    static const Table<R> t{
        "profile",
        "name, id",
        {Col("name", &R::name), Col("rifle_id", &R::rifle_id),
         Col("cartridge_id", &R::cartridge_id), Col("zero_offset_up_m", &R::zero_offset_up_m),
         Col("zero_offset_right_m", &R::zero_offset_right_m),
         Col("velocity_scale", &R::velocity_scale), Col("drag_scale", &R::drag_scale),
         DbDefaultText("created_at", &R::created_at), Col("last_used_at", &R::last_used_at)},
        [](Database& db, const R& r) -> Status {
            if (Status s = Exec(db, "DELETE FROM profile_dsf WHERE profile_id = ?", {r.id}); !s) {
                return s;
            }
            for (const DsfPoint& p : r.dsf) {
                if (Status s = Exec(db,
                                    "INSERT INTO profile_dsf (profile_id, mach, factor) VALUES (?, ?, ?)",
                                    {r.id, p.mach, p.factor});
                    !s) {
                    return s;
                }
            }
            return Ok();
        },
        [](Database& db, R& r) -> Status {
            auto rows = Rows<DsfPoint>(
                db, "SELECT mach, factor FROM profile_dsf WHERE profile_id = ? ORDER BY mach", r.id,
                [](const Statement& st) { return DsfPoint{st.ColumnDouble(0), st.ColumnDouble(1)}; });
            if (!rows) {
                return rows.error();
            }
            r.dsf = std::move(rows).value();
            return Ok();
        }};
    return t;
}

template <>
const Table<ConditionsRecord>& TableOf() {
    using R = ConditionsRecord;
    static const Table<R> t{
        "conditions",
        "name, id",
        {Col("name", &R::name),
         Nested("altitude_m", &R::atmosphere, &Atmosphere::altitude_m),
         Nested("pressure_pa", &R::atmosphere, &Atmosphere::pressure_pa),
         Nested("temperature_k", &R::atmosphere, &Atmosphere::temperature_k),
         Nested("humidity", &R::atmosphere, &Atmosphere::humidity),
         Col("powder_temp_k", &R::powder_temp_k), Col("latitude_rad", &R::latitude_rad),
         Col("azimuth_rad", &R::azimuth_rad), Col("look_angle_rad", &R::look_angle_rad),
         Col("cant_rad", &R::cant_rad), DbDefaultText("created_at", &R::created_at)},
        [](Database& db, const R& r) -> Status {
            if (Status s = Exec(db, "DELETE FROM wind_zone WHERE conditions_id = ?", {r.id}); !s) {
                return s;
            }
            std::int64_t seq = 0;
            for (const WindZone& w : r.winds) {
                if (Status s = Exec(db,
                                    "INSERT INTO wind_zone (conditions_id, seq, until_range_m, "
                                    "speed_mps, from_rad, vertical_mps) VALUES (?, ?, ?, ?, ?, ?)",
                                    {r.id, seq++, w.until_range_m, w.speed_mps, w.from_rad,
                                     w.vertical_mps});
                    !s) {
                    return s;
                }
            }
            return Ok();
        },
        [](Database& db, R& r) -> Status {
            auto rows = Rows<WindZone>(
                db,
                "SELECT until_range_m, speed_mps, from_rad, vertical_mps FROM wind_zone "
                "WHERE conditions_id = ? ORDER BY seq",
                r.id, [](const Statement& st) {
                    return WindZone{st.ColumnDouble(0), st.ColumnDouble(1), st.ColumnDouble(2),
                                    st.ColumnDouble(3)};
                });
            if (!rows) {
                return rows.error();
            }
            r.winds = std::move(rows).value();
            return Ok();
        }};
    return t;
}

template <>
const Table<DopeRecord>& TableOf() {
    using R = DopeRecord;
    static const Table<R> t{
        "dope_log",
        "shot_at, id",
        {Col("profile_id", &R::profile_id), DbDefaultText("shot_at", &R::shot_at),
         Col("range_m", &R::range_m), Col("observed_elevation_rad", &R::observed_elevation_rad),
         Col("observed_windage_rad", &R::observed_windage_rad),
         Col("predicted_elevation_rad", &R::predicted_elevation_rad),
         Col("predicted_windage_rad", &R::predicted_windage_rad),
         Nested("altitude_m", &R::atmosphere, &Atmosphere::altitude_m),
         Nested("pressure_pa", &R::atmosphere, &Atmosphere::pressure_pa),
         Nested("temperature_k", &R::atmosphere, &Atmosphere::temperature_k),
         Nested("humidity", &R::atmosphere, &Atmosphere::humidity),
         Col("powder_temp_k", &R::powder_temp_k), Col("look_angle_rad", &R::look_angle_rad),
         Col("use_for_truing", &R::use_for_truing), Col("notes", &R::notes)},
        nullptr,
        nullptr};
    return t;
}

// Columns written by a save: database-default columns only when set.
template <typename T>
std::vector<const Column<T>*> WrittenColumns(const Table<T>& t, const T& r) {
    std::vector<const Column<T>*> cols;
    for (const auto& c : t.columns) {
        if (c.insert || !AsText(c.get(r)).empty()) {
            cols.push_back(&c);
        }
    }
    return cols;
}

template <typename T>
bool HasNameColumn(const Table<T>& t) {
    for (const auto& c : t.columns) {
        if (std::string(c.name) == "name") {
            return true;
        }
    }
    return false;
}

} // namespace

template <typename T>
Result<Id> Repository<T>::Save(T& record) {
    const Table<T>& t = TableOf<T>();
    const auto cols = WrittenColumns(t, record);
    std::string sql;
    if (record.id == 0) {
        std::string names, params;
        for (const auto* c : cols) {
            names += (names.empty() ? "" : ", ") + std::string(c->name);
            params += (params.empty() ? ":" : ", :") + std::string(c->name);
        }
        sql = "INSERT INTO " + std::string(t.name) + " (" + names + ") VALUES (" + params + ")";
    } else {
        std::string sets;
        for (const auto* c : cols) {
            sets += (sets.empty() ? "" : ", ") + std::string(c->name) + " = :" + c->name;
        }
        sql = "UPDATE " + std::string(t.name) + " SET " + sets + " WHERE id = :id";
    }

    // On failure the record keeps the id it came with.
    struct IdGuard {
        T& r;
        Id original;
        bool done = false;
        ~IdGuard() {
            if (!done) {
                r.id = original;
            }
        }
    } guard{record, record.id};

    // Join the caller's transaction if there is one (SQLite does not nest).
    Transaction txn;
    if (!db_.connection().InTransaction()) {
        auto begun = Transaction::Begin(db_.connection());
        if (!begun) {
            return begun.error();
        }
        txn = std::move(begun).value();
    }
    auto st = Statement::Prepare(db_.connection(), sql);
    if (!st) {
        return st.error();
    }
    for (const auto* c : cols) {
        if (Status s = Bind(st.value(), std::string(":") + c->name, c->get(record)); !s) {
            return s.error();
        }
    }
    if (record.id != 0) {
        if (Status s = st.value().BindInt64(":id", record.id); !s) {
            return s.error();
        }
    }
    if (auto step = st.value().Step(); !step) {
        return step.error();
    }
    if (record.id == 0) {
        record.id = db_.connection().LastInsertRowId();
    } else if (db_.connection().Changes() == 0) {
        return Error(ErrorCode::kNotFound, 0,
                     std::string(t.name) + " " + std::to_string(record.id) + " does not exist");
    }
    if (t.save_children) {
        if (Status s = t.save_children(db_, record); !s) {
            return s.error();
        }
    }
    if (txn.IsActive()) {
        if (Status s = txn.Commit(); !s) {
            return s.error();
        }
    }
    guard.done = true;
    return record.id;
}

template <typename T>
Result<std::optional<T>> Repository<T>::Get(Id id) {
    auto list = [&]() -> Result<std::vector<T>> {
        const Table<T>& t = TableOf<T>();
        std::string sql = "SELECT id";
        for (const auto& c : t.columns) {
            sql += ", " + std::string(c.name);
        }
        sql += " FROM " + std::string(t.name) + " WHERE id = ?";
        return Rows<T>(db_, sql, id, [&t](const Statement& st) {
            T r;
            r.id = st.ColumnInt64(0);
            for (std::size_t i = 0; i < t.columns.size(); ++i) {
                t.columns[i].set(r, Read(st, static_cast<int>(i) + 1));
            }
            return r;
        });
    }();
    if (!list) {
        return list.error();
    }
    if (list.value().empty()) {
        return std::optional<T>{};
    }
    T r = std::move(list.value().front());
    if (const auto& load = TableOf<T>().load_children; load) {
        if (Status s = load(db_, r); !s) {
            return s.error();
        }
    }
    return std::optional<T>(std::move(r));
}

template <typename T>
Result<std::vector<T>> Repository<T>::List(const std::string& name_filter) {
    const Table<T>& t = TableOf<T>();
    std::string sql = "SELECT id";
    for (const auto& c : t.columns) {
        sql += ", " + std::string(c.name);
    }
    sql += " FROM " + std::string(t.name);
    const bool filter = !name_filter.empty() && HasNameColumn(t);
    if (filter) {
        sql += " WHERE instr(lower(name), lower(:filter)) > 0";
    }
    sql += " ORDER BY " + std::string(t.order_by);

    auto st = Statement::Prepare(db_.connection(), sql);
    if (!st) {
        return st.error();
    }
    if (filter) {
        if (Status s = st.value().BindText(":filter", name_filter); !s) {
            return s.error();
        }
    }
    std::vector<T> out;
    while (true) {
        auto step = st.value().Step();
        if (!step) {
            return step.error();
        }
        if (step.value() == Statement::StepResult::kDone) {
            break;
        }
        T r;
        r.id = st.value().ColumnInt64(0);
        for (std::size_t i = 0; i < t.columns.size(); ++i) {
            t.columns[i].set(r, Read(st.value(), static_cast<int>(i) + 1));
        }
        out.push_back(std::move(r));
    }
    if (t.load_children) {
        for (T& r : out) {
            if (Status s = t.load_children(db_, r); !s) {
                return s.error();
            }
        }
    }
    return out;
}

template <typename T>
Status Repository<T>::Remove(Id id) {
    return Exec(db_, "DELETE FROM " + std::string(TableOf<T>().name) + " WHERE id = ?", {id});
}

template class Repository<DragCurveRecord>;
template class Repository<BulletRecord>;
template class Repository<CartridgeRecord>;
template class Repository<RifleRecord>;
template class Repository<ReticleRecord>;
template class Repository<ScopeRecord>;
template class Repository<ProfileRecord>;
template class Repository<ConditionsRecord>;
template class Repository<DopeRecord>;

Result<std::optional<std::string>> GetSetting(Database& db, const std::string& key) {
    auto st = Statement::Prepare(db.connection(), "SELECT value FROM app_setting WHERE key = ?");
    if (!st) {
        return st.error();
    }
    if (Status s = st.value().BindText(1, key); !s) {
        return s.error();
    }
    auto step = st.value().Step();
    if (!step) {
        return step.error();
    }
    if (step.value() == Statement::StepResult::kDone) {
        return std::optional<std::string>{};
    }
    return std::optional<std::string>(st.value().ColumnText(0));
}

Status SetSetting(Database& db, const std::string& key, const std::string& value) {
    return Exec(db,
                "INSERT INTO app_setting (key, value) VALUES (?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
                {key, value});
}

Result<std::optional<std::vector<std::uint8_t>>> GetPhoto(Database& db, const std::string& kind, Id owner) {
    auto st = Statement::Prepare(db.connection(), "SELECT image FROM photo WHERE kind = ? AND owner_id = ?");
    if (!st) {
        return st.error();
    }
    if (Status s = st.value().BindText(1, kind); !s) {
        return s.error();
    }
    if (Status s = st.value().BindInt64(2, owner); !s) {
        return s.error();
    }
    auto step = st.value().Step();
    if (!step) {
        return step.error();
    }
    if (step.value() == Statement::StepResult::kDone) {
        return std::optional<std::vector<std::uint8_t>>{};
    }
    return std::optional<std::vector<std::uint8_t>>(st.value().ColumnBlob(0));
}

Status SetPhoto(Database& db, const std::string& kind, Id owner, const std::vector<std::uint8_t>& image) {
    if (image.empty()) {
        return Exec(db, "DELETE FROM photo WHERE kind = ? AND owner_id = ?", {kind, owner});
    }
    auto st = Statement::Prepare(db.connection(),
                                 "INSERT INTO photo (kind, owner_id, image) VALUES (?, ?, ?) "
                                 "ON CONFLICT(kind, owner_id) DO UPDATE SET image = excluded.image");
    if (!st) {
        return st.error();
    }
    if (Status s = st.value().BindText(1, kind); !s) {
        return s;
    }
    if (Status s = st.value().BindInt64(2, owner); !s) {
        return s;
    }
    if (Status s = st.value().BindBlob(3, image.data(), static_cast<int>(image.size())); !s) {
        return s;
    }
    auto step = st.value().Step();
    if (!step) {
        return step.error();
    }
    return Ok();
}

Result<std::vector<Id>> PhotoOwners(Database& db, const std::string& kind) {
    auto st = Statement::Prepare(db.connection(), "SELECT owner_id FROM photo WHERE kind = ? ORDER BY owner_id");
    if (!st) {
        return st.error();
    }
    if (Status s = st.value().BindText(1, kind); !s) {
        return s.error();
    }
    std::vector<Id> out;
    while (true) {
        auto step = st.value().Step();
        if (!step) {
            return step.error();
        }
        if (step.value() == Statement::StepResult::kDone) {
            return out;
        }
        out.push_back(st.value().ColumnInt64(0));
    }
}

} // namespace ballistics::storage
