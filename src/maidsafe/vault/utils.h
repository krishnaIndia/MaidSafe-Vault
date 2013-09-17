/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.novinet.com/license

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#ifndef MAIDSAFE_VAULT_UTILS_H_
#define MAIDSAFE_VAULT_UTILS_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "boost/filesystem/path.hpp"
#include "leveldb/db.h"

#include "maidsafe/common/error.h"
#include "maidsafe/data_types/data_name_variant.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/parameters.h"

#include "maidsafe/nfs/message_types.h"
#include "maidsafe/nfs/client/messages.h"

#include "maidsafe/vault/types.h"
#include "maidsafe/vault/data_manager/data_manager.h"
#include "maidsafe/vault/version_manager/version_manager.h"
#include "maidsafe/routing/message.h"


namespace maidsafe {

namespace vault {

template<typename T>
class Accumulator;

namespace detail {

template<typename T>
struct RequiredValue {};

template<>
struct RequiredValue<routing::SingleSource> {
  int operator()() {
    return 1;
  }
};

template<>
struct RequiredValue<routing::GroupSource> {
  int operator()() {
    return routing::Parameters::node_group_size - 1;
  }
};

class GetNodeId {
 public:
  template<typename T>
  NodeId operator()(const T& node) {
    return node.data;
  }
};

template<>
NodeId GetNodeId::operator()(const routing::GroupSource& node) {
  return node.sender_id.data;
}

template<typename T>
DataNameVariant GetNameVariant(const T&) {
  T::invalid_parameter;
}

template<>
DataNameVariant GetNameVariant(const nfs_vault::DataName& data) {
   return GetDataNameVariant(data.type, data.raw_name);
}

template<>
DataNameVariant GetNameVariant(const nfs_vault::DataNameAndContent& data) {
   return GetNameVariant(data.name);
}

template<>
DataNameVariant GetNameVariant(const nfs_vault::DataAndPmidHint& data) {
   return GetNameVariant(data.data.name);
}

template<>
DataNameVariant GetNameVariant(const nfs_client::DataAndReturnCode& data) {
   return GetNameVariant(data.data.name);
}

template<>
DataNameVariant GetNameVariant(const nfs_client::DataNameAndContentOrReturnCode& data) {
  if (data.data)
    return GetNameVariant(data.data->name);
  else
    return GetNameVariant(data.data_name_and_return_code->name);
}

template<typename ServiceHandlerType, typename MessageType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const MessageType& message,
                 const Sender& sender,
                 const NodeId& receiver);

template <typename ValidateSender,
          typename AccumulatorType,
          typename Checker,
          typename ServiceHandlerType>
struct OperationHandler {
 public:
  OperationHandler(ValidateSender validate_sender_in,
                   AccumulatorType& accumulator_in,
                   Checker checker_in,
                   ServiceHandlerType* service_in,
                   std::mutex& mutex_in)
      : validate_sender(validate_sender_in),
        accumulator(accumulator_in),
        checker(checker_in),
        service(service_in),
        mutex(mutex_in) {}

  template<typename MessageType, typename Sender, typename Receiver>
  void operator() (const MessageType& message, const Sender& sender, const Receiver& receiver) {
    if (!validate_sender(message, sender))
      return;
    {
      std::lock_guard<std::mutex> lock(mutex);
        if (accumulator.CheckHandled(message))
          return;
      if (accumulator.AddPendingRequest(message, sender, checker) !=
              Accumulator<typename AccumulatorType::type>::AddResult::kSuccess)
        return;
      accumulator.SetHandled(message, sender);
    }
    DoOperation<MessageType, ServiceHandlerType>(service,
                                                 message,
                                                 sender,
                                                 receiver.data);
  }

 private:
  ValidateSender validate_sender;
  AccumulatorType& accumulator;
  Checker checker;
  ServiceHandlerType* service;
  std::mutex& mutex;
};

template<typename ServiceHandlerType, typename MessageType, typename Sender>
void DoOperation(ServiceHandlerType* /*service*/,
                 const MessageType& /*message*/,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  MessageType::Invalid_function_call;
}


// ================================== Account Specialisations ====================================
// CreateAccountRequestFromMaidNodeToMaidManager, Empty

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::CreateAccountRequestFromMaidNodeToMaidManager& /*message*/,
                 const Sender& sender,
                 const NodeId& /*receiver*/) {
  service->CreateAccount(MaidName(sender));
}


// ================================== Delete Specialisations ======================================
//   DeleteRequestFromMaidNodeToMaidManager, DataName
//   DeleteRequestFromMaidManagerToDataManager, DataName
//   DeleteRequestFromDataManagerToPmidManager, DataName
//   DeleteRequestFromPmidManagerToPmidNode, DataName

template <typename ServiceHandlerType>
class DeleteVisitor : public boost::static_visitor<> {
 public:
  DeleteVisitor(ServiceHandlerType* service, const NodeId& sender, const nfs::MessageId& message_id)
      : service_(service),
        kSender(sender),
        kMessageId(message_id) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_->template HandleDelete<typename Name::data_type>(kSender, data_name, kMessageId);
  }
 private:
  ServiceHandlerType* service_;
  NodeId kSender;
  nfs::MessageId kMessageId;
};

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::DeleteRequestFromMaidNodeToMaidManager& message,
                 const Sender& sender,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*(message.contents)));
  DeleteVisitor<ServiceHandlerType> delete_visitor(service,
                                                   detail::GetNodeId(sender),
                                                   message.message_id);
  boost::apply_visitor(delete_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::DeleteRequestFromMaidManagerToDataManager& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  DeleteVisitor<ServiceHandlerType> delete_visitor(service);
  boost::apply_visitor(delete_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::DeleteRequestFromDataManagerToPmidManager& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  DeleteVisitor<ServiceHandlerType> delete_visitor(service);
  boost::apply_visitor(delete_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::DeleteRequestFromPmidManagerToPmidNode& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  DeleteVisitor<ServiceHandlerType> delete_visitor(service);
  boost::apply_visitor(delete_visitor, data_name);
}

// ================================== Get Specialisations ======================================
//   GetCachedResponseFromPmidNodeToMaidNode, DataNameAndContentOrReturnCode
//   GetVersionsResponseFromVersionManagerToMaidNode, StructuredDataNameAndContentOrReturnCode
//   GetBranchResponseFromVersionManagerToMaidNode, StructuredDataNameAndContentOrReturnCode
//   GetVersionsResponseFromVersionManagerToDataGetter, StructuredDataNameAndContentOrReturnCode
//   GetBranchResponseFromVersionManagerToDataGetter, StructuredDataNameAndContentOrReturnCode
//   GetPmidAccountRequestFromPmidNodeToPmidManager, Empty
//   GetPmidHealthRequestFromMaidNodeToMaidManager, DataName
//   GetVersionsRequestFromMaidNodeToVersionManager, DataName
//   GetBranchRequestFromMaidNodeToVersionManager, DataNameAndVersion
//   GetVersionsRequestFromDataGetterToVersionManager, DataName
//   GetBranchRequestFromDataGetterToVersionManager, DataNameAndVersion
//   GetPmidAccountResponseFromPmidManagerToPmidNode, DataNameAndContentOrReturnCode


// ================================= Get Response Specialisations ================================
//   GetResponseFromDataManagerToMaidNode, DataNameAndContentOrReturnCode
//   GetResponseFromDataManagerToDataGetter, DataNameAndContentOrReturnCode
//   GetResponseFromPmidNodeToDataManager, DataNameAndContentOrReturnCode

template <typename ServiceHandlerType>
class GetResponseVisitor : public boost::static_visitor<> {
 public:
  GetResponseVisitor(ServiceHandlerType* service, const NonEmptyString& content)
      : service_(service),
        kContent_(content),
        kError_(CommonErrors::success) {}

  GetResponseVisitor(ServiceHandlerType* service, const maidsafe_error& error)
      : service_(service),
        kError_(error) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    if (kError_.code() == CommonErrors::success)
      service_->template HandleGetResponse<typename Name::data_type>(data_name, kContent_);
    else
      service_->template HandleGetResponse<Name::data_type>(data_name, kError_);
  }
  private:
   ServiceHandlerType* service_;
   NonEmptyString kContent_;
   maidsafe_error kError_;
};

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::GetResponseFromDataManagerToMaidNode& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  if (message.contents->data) {
    auto data_name(detail::GetNameVariant(*message.contents));
    GetResponseVisitor<ServiceHandlerType> get_response_visitor(service,
                                                            message.contents->data->content);
    boost::apply_visitor(get_response_visitor, data_name);
  } else {
    auto data_name(detail::GetNameVariant(*message.contents));
    GetResponseVisitor<ServiceHandlerType> get_response_visitor(
        service, message.contents->data_name_and_return_code->return_code);
    boost::apply_visitor(get_response_visitor, data_name);
  }
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::GetResponseFromDataManagerToDataGetter& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  if (message.contents->data) {
    auto data_name(detail::GetNameVariant(*message.contents));
    GetResponseVisitor<ServiceHandlerType> get_response_visitor(service,
                                               message.contents->data->content);
    boost::apply_visitor(get_response_visitor, data_name);
  } else {
    auto data_name(detail::GetNameVariant(*message.contents));
    GetResponseVisitor<ServiceHandlerType> get_response_visitor(
        service, message.contents->data_name_and_return_code->return_code);
    boost::apply_visitor(get_response_visitor, data_name);
  }
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::GetResponseFromPmidNodeToDataManager& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  if (message.contents->data) {
    auto data_name(detail::GetNameVariant(*message.contents));
    GetResponseVisitor<ServiceHandlerType> get_response_visitor(service,
                                                         message.contents->data->content);
    boost::apply_visitor(get_response_visitor, data_name);
  } else {
    auto data_name(detail::GetNameVariant(*message.contents));
    GetResponseVisitor<ServiceHandlerType> get_response_visitor(
        service, message.contents->data_name_and_return_code->return_code);
    boost::apply_visitor(get_response_visitor, data_name);
  }
}

// ================================== Get Request Specialisations =================================
//   GetRequestFromMaidNodeToDataManager, DataName
//   GetRequestFromDataManagerToPmidNode, DataName
//   GetRequestFromDataGetterToDataManager, DataName
//   GetRequestFromPmidNodeToDataManager, DataName

template <typename ServiceType>
class GetRequestVisitor : public boost::static_visitor<> {
 public:
  GetRequestVisitor(ServiceType& service)
      : service_(service) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_.HandleGet<Name::data_type>(data_name);
  }
  private:
   ServiceType& service_;
};

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::GetRequestFromMaidNodeToDataManager& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  GetRequestVisitor<ServiceHandlerType> get_visitor(service);
  boost::apply_visitor(get_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::GetRequestFromPmidNodeToDataManager& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  GetRequestVisitor<ServiceHandlerType> get_visitor(service);
  boost::apply_visitor(get_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::GetRequestFromDataManagerToPmidNode& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  GetRequestVisitor<ServiceHandlerType> get_visitor(service);
  boost::apply_visitor(get_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::GetRequestFromDataGetterToDataManager& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  GetRequestVisitor<ServiceHandlerType> get_visitor(service);
  boost::apply_visitor(get_visitor, data_name);
}

// ================================== Put Specialisations ======================================

template <typename ServiceHandlerType>
class HintedPutVisitor : public boost::static_visitor<> {
 public:
  HintedPutVisitor(ServiceHandlerType* service,
                   const NonEmptyString& content,
                   const NodeId& sender,
                   const Identity pmid_hint,
                   const nfs::MessageId& message_id)
      : service_(service),
        kContent_(content),
        kSender(sender),
        kPmidHint_(pmid_hint),
        kMessageId(message_id) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_->template HandlePut(MaidName(kSender),
                                 Name::data_type(data_name, kContent_),
                                 kPmidHint_,
                                 kMessageId);
  }
  private:
   ServiceHandlerType* service_;
   NonEmptyString kContent_;
   Identity kPmidHint_;
   nfs::MessageId kMessageId;
   NodeId kSender;
};

template <typename ServiceHandlerType>
class DataManagerPutVisitor : public boost::static_visitor<> {
 public:
  DataManagerPutVisitor(ServiceHandlerType* service,
                        const NonEmptyString& content,
                        const Identity& maid_name,
                        const Identity& pmid_name,
                        const nfs::MessageId& message_id)
      : service_(service),
        kContent_(content),
        kMaidName(maid_name),
        kPmidName_(pmid_name) ,
        kMessageId(message_id) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_->template HandlePut(Name::data_type(data_name, kContent_), kMaidName, kPmidName_,
                                 kMessageId);
  }
 private:
  ServiceHandlerType* service_;
  NonEmptyString kContent_;
  const MaidName kMaidName;
  const PmidName kPmidName_;
  const nfs::MessageId kMessageId;
};

template <typename ServiceHandlerType>
class PmidNodePutVisitor : public boost::static_visitor<> {
 public:
  PmidNodePutVisitor(ServiceHandlerType* service,
                     const NonEmptyString& content,
                     const nfs::MessageId& message_id)
      : service_(service),
        kContent_(content),
        kMessageId(message_id) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_->template HandlePut(Name::data_type(data_name, kContent_), kMessageId);
  }
 private:
  ServiceHandlerType* service_;
  NonEmptyString kContent_;
  const nfs::MessageId kMessageId;
};


template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::PutRequestFromDataManagerToPmidManager& message,
                 const Sender& /*sender*/,
                 const NodeId& receiver) {
  auto data_name(detail::GetNameVariant(*message.contents));
  DataManagerPutVisitor<ServiceHandlerType> put_visitor(service,
                                                        message.contents->content,
                                                        message.message_id,
                                                        receiver);
  boost::apply_visitor(put_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::PutRequestFromPmidManagerToPmidNode& message,
                 const Sender& /*sender*/,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  PmidNodePutVisitor<ServiceHandlerType> put_visitor(service,
                                                     message.contents->content,
                                                     message.message_id);
  boost::apply_visitor(put_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::PutRequestFromMaidManagerToDataManager& message,
                 const Sender& sender,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  DataManagerPutVisitor<ServiceHandlerType> put_visitor(service,
                                                        message.contents->data.content,
                                                        sender.group_id,
                                                        message.contents->pmid_hint,
                                                        message.message_id);
  boost::apply_visitor(put_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::PutRequestFromMaidNodeToMaidManager& message,
                 const Sender& sender,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  HintedPutVisitor<ServiceHandlerType> put_visitor(service,
                                                   message.contents->data.content,
                                                   detail::GetNodeId(sender),
                                                   message.contents->pmid_hint,
                                                   message.message_id);
  boost::apply_visitor(put_visitor, data_name);
}



// ================ Put Response Specialisations ===================================================
// PutResponseFromPmidManagerToDataManager, DataNameAndContentAndReturnCode
// PutResponseFromDataManagerToMaidManager, DataNameAndContent
// PutResponseFromPmidNodeToPmidManager, DataNameAndContentAndReturnCode

template <typename ServiceHandlerType>
class PutResponseVisitor : public boost::static_visitor<> {
 public:
  PutResponseVisitor(ServiceHandlerType* service,
                     const NonEmptyString& content,
                     const Identity& pmid_node,
                     const nfs::MessageId& message_id,
                     const maidsafe_error& return_code)
      : service_(service),
        kContent_(content),
        kPmidNode_(pmid_node),
        kMessageId(message_id),
        kReturnCode_(return_code) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_->template HandlePutResponse(Name::data_type(data_name, kContent_),
                                         PmidName(kSender),
                                         kMessageId,
                                         maidsafe_error(kReturnCode_));
  }
  private:
   ServiceHandlerType* service_;
   const NonEmptyString kContent_;
   const PmidName kPmidNode_;
   const nfs::MessageId kMessageId;
   const NodeId kSender;
   const maidsafe_error kReturnCode_;
};

template <typename ServiceHandlerType>
class PutResponseSuccessVisitor : public boost::static_visitor<> {
 public:
  PutResponseSuccessVisitor(ServiceHandlerType* service,
                            const Identity& pmid_node,
                            const nfs::MessageId& message_id,
                            const maidsafe_error& return_code)
      : service_(service),
        kPmidNode_(pmid_node),
        kMessageId(message_id),
        kReturnCode_(return_code) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_->template HandlePutResponse<typename Name::data_type>(data_name,
                                                                   PmidName(kSender),
                                                                   kMessageId,
                                                                   maidsafe_error(kReturnCode_));
  }
  private:
   ServiceHandlerType* service_;
   const PmidName kPmidNode_;
   const nfs::MessageId kMessageId;
   const NodeId kSender;
   const maidsafe_error kReturnCode_;
};


template <typename ServiceHandlerType>
class MaidManagerPutResponseVisitor : public boost::static_visitor<> {
 public:
  MaidManagerPutResponseVisitor(ServiceHandlerType* service,
                                const Identity& maid_node,
                                const int32_t& cost)
      : service_(service),
        kMaidNode_(maid_node),
        kCost_(cost) {}

  template <typename Name>
  void operator()(const Name& data_name) {
    service_->template HandlePutResponse(kMaidNode_, data_name, kCost_);
  }
  private:
   ServiceHandlerType* service_;
   const MaidName kMaidNode_;
   const int32_t kCost_;
};


template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::PutResponseFromDataManagerToMaidManager& message,
                 const Sender& /*sender*/,
                 const NodeId& receiver) {
  auto data_name(detail::GetNameVariant(message.contents->name));
  MaidManagerPutResponseVisitor<ServiceHandlerType> put_response_visitor(
      service,
      receiver,
      message.contents->cost,
      message.message_id);
  boost::apply_visitor(put_response_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::PutResponseFromPmidNodeToPmidManager& message,
                 const Sender& sender,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  PutResponseVisitor<ServiceHandlerType> put_visitor(service,
                                                     message.contents->content,
                                                     sender,
                                                     message.message_id,
                                                     message.contents->return_code);
  boost::apply_visitor(put_visitor, data_name);
}

template<typename ServiceHandlerType, typename Sender>
void DoOperation(ServiceHandlerType* service,
                 const nfs::PutResponseFromPmidManagerToDataManager& message,
                 const Sender& sender,
                 const NodeId& /*receiver*/) {
  auto data_name(detail::GetNameVariant(*message.contents));
  if (message.contents->content) {
    PutResponseVisitor<ServiceHandlerType> put_visitor(service,
                                                       message.contents->content,
                                                       PmidName(Identity(sender.group_id)),
                                                       message.message_id,
                                                       message.contents->return_code);
    boost::apply_visitor(put_visitor, data_name);
  } else {
    PutResponseSuccessVisitor<ServiceHandlerType> put_visitor(service,
                                                              PmidName(Identity(sender.group_id)),
                                                              message.message_id,
                                                              message.contents->return_code);
    boost::apply_visitor(put_visitor, data_name);
  }
}


// =============================================================================================


template <typename MessageType>
struct ValidateSenderType {
  typedef std::function<bool(const MessageType&, const typename MessageType::Sender&)> type;
};


void InitialiseDirectory(const boost::filesystem::path& directory);
//bool ShouldRetry(routing::Routing& routing, const nfs::Message& message);

template<typename Data>
bool IsDataElement(const typename Data::Name& name, const DataNameVariant& data_name_variant);

//void SendReply(const nfs::Message& original_message,
//               const maidsafe_error& return_code,
//               const routing::ReplyFunctor& reply_functor);

template<typename AccountSet, typename Account>
typename Account::serialised_type GetSerialisedAccount(
    std::mutex& mutex,
    const AccountSet& accounts,
    const typename Account::Name& account_name);

template<typename AccountSet, typename Account>
typename Account::serialised_info_type GetSerialisedAccountSyncInfo(
    std::mutex& mutex,
    const AccountSet& accounts,
    const typename Account::Name& account_name);

// To be moved to Routing
bool operator ==(const routing::GroupSource& lhs,  const routing::GroupSource& rhs);

/* Commented by Mahmoud on 2 Sep -- It may be of no use any more
// Returns true if the required successful request count has been reached
template<typename Accumulator>
bool AddResult(const nfs::Message& message,
               const routing::ReplyFunctor& reply_functor,
               const maidsafe_error& return_code,
               Accumulator& accumulator,
               std::mutex& accumulator_mutex,
               int requests_required);
*/

}  // namespace detail

template <typename ServiceHandler,
          typename MessageType,
          typename AccumulatorVariantType>
struct OperationHandlerWrapper {
  typedef detail::OperationHandler<
              typename detail::ValidateSenderType<MessageType>::type,
              Accumulator<AccumulatorVariantType>,
              typename Accumulator<AccumulatorVariantType>::AddCheckerFunctor,
              ServiceHandler> TypedOperationHandler;

  OperationHandlerWrapper(Accumulator<AccumulatorVariantType>& accumulator,
                          typename detail::ValidateSenderType<MessageType>::type validate_sender,
                          typename Accumulator<AccumulatorVariantType>::AddCheckerFunctor checker,
                          ServiceHandler* service,
                          std::mutex& mutex)
      : typed_operation_handler(validate_sender, accumulator, checker, service, mutex) {}

  void operator() (const MessageType& message,
                   const typename MessageType::Sender& sender,
                   const typename MessageType::Receiver& receiver) {
    typed_operation_handler(message, sender, receiver);
  }

 private:
  TypedOperationHandler typed_operation_handler;
};

template <typename T>
struct RequiredRequests {
  int operator()(const T& /*sender*/) {
    return detail::RequiredValue<T>()();
  }
};


//template<typename Message>
//inline bool FromMaidManager(const Message& message);
//
//template<typename Message>
//inline bool FromDataManager(const Message& message);
//
//template<typename Message>
//inline bool FromPmidManager(const Message& message);
//
//template<typename Message>
//inline bool FromDataHolder(const Message& message);
//
//template<typename Message>
//inline bool FromClientMaid(const Message& message);
//
//template<typename Message>
//inline bool FromClientMpid(const Message& message);
//
//template<typename Message>
//inline bool FromOwnerDirectoryManager(const Message& message);
//
//template<typename Message>
//inline bool FromGroupDirectoryManager(const Message& message);
//
//template<typename Message>
//inline bool FromWorldDirectoryManager(const Message& message);
//
//template<typename Message>
//inline bool FromDataGetter(const Message& message);
//
//template<typename Message>
//inline bool FromVersionManager(const nfs::Message& message);

/* Commented by Mahmoud on 2 Sep -- It may be of no use any more
template<typename Persona>
typename Persona::DbKey GetKeyFromMessage(const nfs::Message& message) {
  if (!message.data().type)
    ThrowError(CommonErrors::parsing_error);
  return GetDataNameVariant(*message.data().type, message.data().name);
}
*/
std::unique_ptr<leveldb::DB> InitialiseLevelDb(const boost::filesystem::path& db_path);

}  // namespace vault

}  // namespace maidsafe

#include "maidsafe/vault/utils-inl.h"

#endif  // MAIDSAFE_VAULT_UTILS_H_
