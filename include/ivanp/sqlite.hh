#ifndef IVANP_SQLITE_HH
#define IVANP_SQLITE_HH

#include <iostream>
#include <iomanip>
#include <sstream>

#include "sqlite3.h"
// https://sqlite.org/cintro.html

#include "ivanp/string.hh"

#define STR1(x) #x
#define STR(x) STR1(x)

#define THROW_SQLITE_MSG(X,MSG) \
  throw sqlite_error(STR(__LINE__) ": " X "(): ",MSG)
#define THROW_SQLITE(X) THROW_SQLITE_MSG(X,errmsg())

namespace ivanp {

struct sqlite_error: std::runtime_error {
  using std::runtime_error::runtime_error;
  template <typename... T>
  sqlite_error(T&&... x): runtime_error(ivanp::cat(std::forward<T>(x)...)) { }
  sqlite_error(const char* str): std::runtime_error(str) { }
};

class sqlite {
  sqlite3* db = nullptr;

public:
  const char* errmsg() const noexcept { return sqlite3_errmsg(db); }

  sqlite() = default;
  sqlite(const char* filename) {
    if (sqlite3_open(filename,&db) != SQLITE_OK)
      THROW_SQLITE("sqlite3_open");
  }
  ~sqlite() {
    if (sqlite3_close(db) != SQLITE_OK)
      std::cerr << STR(__LINE__) ": sqlite3_close(): "
        << errmsg() << std::endl;
  }
  sqlite(const sqlite&) = delete;
  sqlite& operator=(const sqlite&) = delete;
  sqlite& operator=(sqlite&& o) noexcept {
    std::swap(db,o.db);
    return *this;
  }
  sqlite(sqlite&& o) noexcept { std::swap(db,o.db); }

  sqlite3* operator+() noexcept { return db; }
  const sqlite3* operator+() const noexcept { return db; }

  class value {
    sqlite3_value* p = nullptr;

  public:
    value() noexcept = default;
    value(sqlite3_value* p) noexcept: p(sqlite3_value_dup(p)) { }
    ~value() { sqlite3_value_free(p); }
    value(const value& o) noexcept: value(o.p) { }
    value(value&& o) noexcept { std::swap(p,o.p); }
    value& operator=(const value& o) noexcept {
      p = sqlite3_value_dup(o.p);
      return *this;
    }
    value& operator=(value&& o) noexcept {
      std::swap(p,o.p);
      return *this;
    }

    sqlite3_value* operator+() noexcept { return p; }
    const sqlite3_value* operator+() const noexcept { return p; }

    int type() const noexcept { return sqlite3_value_type(p); }
    int bytes() const noexcept { return sqlite3_value_bytes(p); }
    int as_int() const noexcept { return sqlite3_value_int(p); }
    sqlite3_int64 as_int64() const noexcept { return sqlite3_value_int64(p); }
    double as_double() const noexcept { return sqlite3_value_double(p); }
    const void* as_blob() const noexcept { return sqlite3_value_blob(p); }
    const char* as_text() const noexcept {
      return reinterpret_cast<const char*>(sqlite3_value_text(p));
    }

    template <typename T> requires std::is_integral_v<T>
    T as() const noexcept {
      if constexpr (sizeof(T) < 8) return as_int();
      else return as_int64();
    }
    template <typename T> requires std::is_floating_point_v<T>
    T as() const noexcept { return as_double(); }
    template <typename T> requires std::is_constructible_v<T,const char*>
    T as() const noexcept(std::is_nothrow_constructible_v<T,const char*>)
    { return T(as_text()); }
    template <typename T> requires std::is_same_v<T,const void*>
    T as() const noexcept { return as_blob(); }

    bool operator==(const value& o) const noexcept {
      const auto len = bytes();
      return (o.bytes() == len) && !memcmp(as_blob(),o.as_blob(),len);
    }
    bool operator<(const value& o) const noexcept {
      const auto len = bytes();
      const auto len_o = o.bytes();
      auto cmp = memcmp(as_blob(),o.as_blob(),(len < len_o ? len : len_o));
      return cmp ? (cmp < 0) : (len < len_o);
    }
    bool operator!=(const value& o) const noexcept { return !((*this) == o); }

    bool operator!() const noexcept {
      return !p || type() == SQLITE_NULL;
    }
  };

  class stmt {
    sqlite3_stmt *p = nullptr;

  public:
    sqlite3* db_handle() const noexcept { return sqlite3_db_handle(p); }
    const char* errmsg() const noexcept { return sqlite3_errmsg(db_handle()); }

    stmt(sqlite3 *db, const char* sql, bool persist=false) {
      if (sqlite3_prepare_v3(
        db, sql, -1,
        persist ? SQLITE_PREPARE_PERSISTENT : 0,
        &p, nullptr
      ) != SQLITE_OK) THROW_SQLITE("sqlite3_prepare_v3");
    }
    stmt(sqlite3 *db, std::string_view sql, bool persist=false) {
      if (sqlite3_prepare_v3(
        db, sql.data(), sql.size(),
        persist ? SQLITE_PREPARE_PERSISTENT : 0,
        &p, nullptr
      ) != SQLITE_OK) THROW_SQLITE("sqlite3_prepare_v3");
    }
    ~stmt() {
      if (sqlite3_finalize(p) != SQLITE_OK)
      std::cerr << STR(__LINE__) ": sqlite3_finalize(): "
        << errmsg() << std::endl;
    }

    stmt(const stmt&) = delete;
    stmt& operator=(const stmt&) = delete;
    stmt& operator=(stmt&& o) noexcept {
      std::swap(p,o.p);
      return *this;
    }
    stmt(stmt&& o) noexcept { std::swap(p,o.p); }

    sqlite3_stmt* operator+() noexcept { return p; }
    const sqlite3_stmt* operator+() const noexcept { return p; }

    bool step() {
      switch (sqlite3_step(p)) {
        case SQLITE_ROW: return true;
        case SQLITE_DONE: return false;
        default: THROW_SQLITE("sqlite3_step");
      }
    }
    stmt& reset() {
      if (sqlite3_reset(p) != SQLITE_OK)
        THROW_SQLITE("sqlite3_reset");
      return *this;
    }
    stmt& clear() {
      if (sqlite3_clear_bindings(p) != SQLITE_OK)
        THROW_SQLITE("sqlite3_clear_bindings");
      return *this;
    }

    // bind ---------------------------------------------------------
    stmt& bind(int i, double x) {
      if (sqlite3_bind_double(p, i, x) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_double");
      return *this;
    }
    template <typename T>
    stmt& bind(int i, T x) requires std::is_integral_v<T> {
      if constexpr (sizeof(T) < 8) {
        if (sqlite3_bind_int(p, i, x) != SQLITE_OK)
          THROW_SQLITE("sqlite3_bind_int");
      } else {
        if (sqlite3_bind_int64(p, i, x) != SQLITE_OK)
          THROW_SQLITE("sqlite3_bind_int64");
      }
      return *this;
    }
    stmt& bind(int i) {
      if (sqlite3_bind_null(p, i) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_null");
      return *this;
    }
    stmt& bind(int i, std::nullptr_t) {
      if (sqlite3_bind_null(p, i) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_null");
      return *this;
    }
    stmt& bind(int i, const char* x, int n=-1, bool trans=true) {
      if (sqlite3_bind_text(p, i, x, n,
            trans ? SQLITE_TRANSIENT : SQLITE_STATIC
      ) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_text");
      return *this;
    }
    stmt& bind(int i, std::string_view x, bool trans=true) {
      if (sqlite3_bind_text(p, i, x.data(), x.size(),
            trans ? SQLITE_TRANSIENT : SQLITE_STATIC
      ) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_text");
      return *this;
    }
    stmt& bind(int i, const void* x, int n, bool trans=true) {
      if (sqlite3_bind_blob(p, i, x, n,
            trans ? SQLITE_TRANSIENT : SQLITE_STATIC
      ) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_blob");
      return *this;
    }
    stmt& bind(int i, std::nullptr_t, int n) {
      if (sqlite3_bind_zeroblob(p, i, n) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_zeroblob");
      return *this;
    }
    stmt& bind(int i, const value& x) {
      if (sqlite3_bind_value(p, i, +x) != SQLITE_OK)
        THROW_SQLITE("sqlite3_bind_value");
      return *this;
    }

    template <typename... T>
    stmt& bind_all(T&&... x) {
      int i = 0;
      return (bind(++i,std::forward<T>(x)), ...);
    }

    // column -------------------------------------------------------
    int column_count() noexcept {
      return sqlite3_column_count(p);
    }
    double column_double(int i) noexcept {
      return sqlite3_column_double(p, i);
    }
    int column_int(int i) noexcept {
      return sqlite3_column_int(p, i);
    }
    sqlite3_int64 column_int64(int i) noexcept {
      return sqlite3_column_int64(p, i);
    }
    const char* column_text(int i) noexcept {
      return reinterpret_cast<const char*>(sqlite3_column_text(p, i));
    }
    const void* column_blob(int i) noexcept {
      return sqlite3_column_blob(p, i);
    }
    value column_value(int i) noexcept {
      return sqlite3_column_value(p, i);
    }
    int column_bytes(int i) noexcept {
      return sqlite3_column_bytes(p, i);
    }
    int column_type(int i) noexcept {
      // SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_BLOB, or SQLITE_NULL
      return sqlite3_column_type(p, i);
    }
    const char* column_name(int i) noexcept {
      return sqlite3_column_name(p, i);
    }

    template <typename T> requires std::is_integral_v<T>
    T column(int i) noexcept {
      if constexpr (sizeof(T) < 8) return column_int(i);
      else return column_int64(i);
    }
    template <typename T> requires std::is_floating_point_v<T>
    T column(int i) noexcept { return column_double(i); }
    template <typename T> requires std::is_constructible_v<T,const char*>
    T column(int i) noexcept(std::is_nothrow_constructible_v<T,const char*>)
    { return T(column_text(i)); }
    template <typename T> requires std::is_same_v<T,const void*>
    T column(int i) noexcept { return column_blob(i); }

    std::string json() {
      std::stringstream ss;
      ss << '[';
      for (int i=0, n=column_count(); i<n; ++i) {
        if (i) ss << ',';
        switch (column_type(i)) {
          case SQLITE_INTEGER:
            ss << column_int(i);
            break;
          case SQLITE_FLOAT:
            ss << column_double(i);
            break;
          case SQLITE_TEXT:
            ss << std::quoted(column_text(i));
            break;
          case SQLITE_BLOB: {
            throw sqlite_error(STR(__LINE__)": blob encoding not implemented");
            // ss << '"' << base64_encode(
            //   reinterpret_cast<const char*>(column_blob(i)),
            //   column_bytes(i)
            // ) << '"';
          }; break;
          case SQLITE_NULL:
            ss << "null";
            break;
        }
      }
      ss << ']';
      return std::move(ss).str();
    }
  };

  stmt prepare(auto sql, bool persist=false) { return { db, sql, persist }; }

  template <typename F>
  sqlite& exec(const char* sql, F&& f) {
    char* err;
    if (sqlite3_exec(db,sql,
      +[](
        void* arg, // 4th argument of sqlite3_exec()
        int ncol, // number of columns
        char** row, // pointers to strings obtained as if from sqlite3_column_text()
        char** cols_names // names of columns
      ) -> int {
        (*reinterpret_cast<F*>(arg))(ncol,row,cols_names);
        return 0;
      }, reinterpret_cast<void*>(&f), &err
    ) != SQLITE_OK) THROW_SQLITE_MSG("sqlite3_exec",err);
    return *this;
  }
  sqlite& exec(const char* sql) {
    char* err;
    if (sqlite3_exec(db,sql,nullptr,nullptr,&err) != SQLITE_OK)
      THROW_SQLITE_MSG("sqlite3_exec",err);
    return *this;
  }
  template <typename... F> requires (sizeof...(F)<2)
  sqlite& operator()(const char* sql, F&&... f) {
    return exec(sql,std::forward<F>(f)...);
  }
};

} // end namespace ivanp

#endif
