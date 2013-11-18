/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */


#include "maidsafe/vault/tests/tests_utils.h"

namespace maidsafe {

namespace vault {

namespace test {

passport::Maid MakeMaid() {
  passport::Anmaid anmaid;
  return passport::Maid(anmaid);
}

passport::Pmid MakePmid() {
 return passport::Pmid(MakeMaid());
}

passport::PublicPmid MakePublicPmid() {
  passport::Pmid pmid(MakePmid());
  return passport::PublicPmid(pmid);
}

template <>
nfs_vault::DataNameAndContent CreateContent<nfs_vault::DataNameAndContent>() {
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  return nfs_vault::DataNameAndContent(data);
}

template <>
nfs_client::DataNameAndSpaceAndReturnCode
CreateContent<nfs_client::DataNameAndSpaceAndReturnCode>() {
  ImmutableData data(NonEmptyString(RandomString(128)));
  nfs_client::ReturnCode return_code(VaultErrors::not_enough_space);
  return nfs_client::DataNameAndSpaceAndReturnCode(data.name(), 100, return_code);
}

template <>
nfs_vault::DataName CreateContent<nfs_vault::DataName>() {
  ImmutableData data(NonEmptyString(RandomString(128)));
  return nfs_vault::DataName(ImmutableData::Name(data.name()));
}

template <>
nfs_vault::AvailableSize CreateContent<nfs_vault::AvailableSize>() {
  return nfs_vault::AvailableSize(2^20);
}

template <>
nfs_vault::Empty CreateContent<nfs_vault::Empty>() {
  return nfs_vault::Empty();
}

template <>
nfs_vault::DataAndPmidHint CreateContent<nfs_vault::DataAndPmidHint>() {
  ImmutableData data(NonEmptyString(RandomString(128)));
  return nfs_vault::DataAndPmidHint(nfs_vault::DataName(data.name()), data.data(),
                                    Identity(RandomString(64)));
}

template <>
nfs_vault::DataNameAndSize CreateContent<nfs_vault::DataNameAndSize>() {
  return nfs_vault::DataNameAndSize(DataTagValue::kImmutableDataValue, Identity(RandomString(64)),
                                    kTestChunkSize);
}

template <>
nfs_client::DataNameAndReturnCode CreateContent<nfs_client::DataNameAndReturnCode>() {
  return nfs_client::DataNameAndReturnCode(nfs_vault::DataName(DataTagValue::kImmutableDataValue,
                                                               Identity(RandomString(64))),
                                           nfs_client::ReturnCode(
                                               CommonErrors::unable_to_handle_request));
}

template <>
nfs_client::DataNameAndContentOrReturnCode
CreateContent<nfs_client::DataNameAndContentOrReturnCode>() {
  return nfs_client::DataNameAndContentOrReturnCode(
             ImmutableData(NonEmptyString(RandomString(2^10))));
}

template <>
nfs_vault::DataNameAndContentOrCheckResult
CreateContent<nfs_vault::DataNameAndContentOrCheckResult>() {
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  return nfs_vault::DataNameAndContentOrCheckResult(data.name(), data.data());
}

template <>
nfs_vault::DataNameAndCost CreateContent<nfs_vault::DataNameAndCost>() {
  return nfs_vault::DataNameAndCost(ImmutableData::Tag::kValue, Identity(RandomString(64)),
                                    kTestChunkSize);
}

template <>
nfs_vault::DataNameAndVersion CreateContent<nfs_vault::DataNameAndVersion>() {
  return nfs_vault::DataNameAndVersion(
             nfs_vault::DataName(ImmutableData::Tag::kValue, Identity(RandomString(64))),
             StructuredDataVersions::VersionName(RandomInt32(),
                                                 ImmutableData::Name(Identity(RandomString(64)))));
}

template <>
nfs_vault::DataNameOldNewVersion CreateContent<nfs_vault::DataNameOldNewVersion>() {
  ImmutableData::Name name(Identity(RandomString(64)));
  return nfs_vault::DataNameOldNewVersion(nfs_vault::DataName(name),
                                          StructuredDataVersions::VersionName(1, name),
                                          StructuredDataVersions::VersionName(2, name));
}

template <>
std::vector<routing::GroupSource> CreateGroupSource(const NodeId& group_id) {
  std::vector<routing::GroupSource> group_source;
  for (auto index(0); index < routing::Parameters::node_group_size; ++index)
    group_source.push_back(routing::GroupSource(routing::GroupId(group_id),
                                                routing::SingleId(NodeId(NodeId::kRandomId))));
  return group_source;
}

protobuf::Sync CreateProtoSync(nfs::MessageAction action_type,
                               const std::string& serialised_action) {
  protobuf::Sync proto_sync;
  proto_sync.set_action_type(static_cast<int>(action_type));
  proto_sync.set_serialised_unresolved_action(serialised_action);
  return proto_sync;
}

}  // namespace test

}  // namespace vault

}  // namespace maidsafe