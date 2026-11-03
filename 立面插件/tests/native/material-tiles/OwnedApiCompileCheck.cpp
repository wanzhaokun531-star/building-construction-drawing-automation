#include "NativeMaterialTiles.h"
#include <type_traits>
#include <utility>

template<class T,class=void> struct BorrowedClone : std::false_type {};
template<class T> struct BorrowedClone<T,std::void_t<decltype(
    std::declval<const T&>().cloneToEmpty(std::declval<AcDbDatabase&>()))>> : std::true_type {};
template<class T,class=void> struct SuppliedExportTarget : std::false_type {};
template<class T> struct SuppliedExportTarget<T,std::void_t<decltype(
    std::declval<const T&>().cloneIntoNewOwnedDb(std::declval<AcDbDatabase&>()))>> : std::true_type {};

static_assert(!BorrowedClone<bce::PreparedMaterialTiles>::value,"borrowed writer must not be exposed");
static_assert(!SuppliedExportTarget<bce::PreparedMaterialTiles>::value,"owned export cannot accept any external database");
static_assert(!std::is_copy_constructible<bce::OwnedMaterialDatabase>::value,"database ownership cannot be copied");
static_assert(std::is_move_constructible<bce::OwnedMaterialDatabase>::value,"database ownership must be transferable");
using OwnedExportSignature=bce::Result<bce::OwnedMaterialDatabase>(bce::PreparedMaterialTiles::*)()const;
static_assert(std::is_same<decltype(&bce::PreparedMaterialTiles::cloneIntoNewOwnedDb),OwnedExportSignature>::value,"exact no-target API contract");
