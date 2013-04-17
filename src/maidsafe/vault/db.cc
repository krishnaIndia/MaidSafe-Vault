/***************************************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                                            *
 *                                                                                                 *
 *  The following source code is property of MaidSafe.net limited and is not meant for external    *
 *  use.  The use of this code is governed by the licence file licence.txt found in the root of    *
 *  this directory and also on www.maidsafe.net.                                                   *
 *                                                                                                 *
 *  You are not free to copy, amend or otherwise use this source code without the explicit         *
 *  written permission of the board of directors of MaidSafe.net.                                  *
 **************************************************************************************************/


#include "maidsafe/vault/db.h"

#include "boost/filesystem/operations.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/passport/types.h"
#include "maidsafe/data_types/data_type_values.h"
#include "maidsafe/data_types/data_name_variant.h"

namespace maidsafe {

namespace vault {

std::unique_ptr<leveldb::DB> Db::leveldb_ = nullptr;
std::atomic<uint32_t> Db::last_account_id_(0);

Db::Db(const boost::filesystem::path& path) {
  std::call_once(flag, [&path](){
      if (boost::filesystem::exists(path))
        boost::filesystem::remove_all(path);
      leveldb::DB* db;
      leveldb::Options options;
      options.create_if_missing = true;
      leveldb::Status status(leveldb::DB::Open(options, path.string(), &db));
      if (!status.ok())
        ThrowError(VaultErrors::failed_to_handle_request);
      leveldb_ = std::move(std::unique_ptr<leveldb::DB>(db));
      assert(leveldb_);
    });
  account_id_ = ++last_account_id_;
}

void Db::Put(const DataNameVariant& key, const NonEmptyString& value) {
  auto result(boost::apply_visitor(GetTagValueAndIdentityVisitor(), key));
  std::string db_key(std::to_string(account_id_) +
                     EncodeToBase32(result.second.string()) +
                     std::to_string(static_cast<uint32_t>(result.first)));
  leveldb::Status status(leveldb_->Put(leveldb::WriteOptions(), db_key, value.string()));
  if (!status.ok())
    ThrowError(VaultErrors::failed_to_handle_request);
}

void Db::Delete(const DataNameVariant& key) {
  auto result(boost::apply_visitor(GetTagValueAndIdentityVisitor(), key));
  std::string db_key(std::to_string(account_id_) +
                     EncodeToBase32(result.second.string()) +
                     std::to_string(static_cast<uint32_t>(result.first)));
  leveldb::Status status(leveldb_->Delete(leveldb::WriteOptions(), db_key));
  if (!status.ok())
    ThrowError(VaultErrors::failed_to_handle_request);
}

NonEmptyString Db::Get(const DataNameVariant& key) {
  auto result(boost::apply_visitor(GetTagValueAndIdentityVisitor(), key));
  std::string db_key(std::to_string(account_id_) +
                     EncodeToBase32(result.second.string()) +
                     std::to_string(static_cast<uint32_t>(result.first)));
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  std::string value;
  leveldb::Status status(leveldb_->Get(read_options, db_key, &value));
  if (!status.ok())
    ThrowError(VaultErrors::failed_to_handle_request);
  assert(!value.empty());
  return NonEmptyString(value);
}

}  // namespace vault

}  // namespace maidsafe