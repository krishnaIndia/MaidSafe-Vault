 /*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/test.h"

#include "maidsafe/vault/demultiplexer.h"

#include "maidsafe/nfs/message.h"

namespace maidsafe {

namespace vault {

// TODO(Alison) - replace these with actual objects
class MaidAccountHolder {
 public:
  MaidAccountHolder();
  virtual ~MaidAccountHolder();
  virtual void HandleMessage(nfs::Message message);
};
class MetadataManager {
 public:
  MetadataManager();
  virtual ~MetadataManager();
  virtual void HandleMessage(nfs::Message message);
};
class PmidAccountHolder {
 public:
  PmidAccountHolder();
  virtual ~PmidAccountHolder();
  virtual void HandleMessage(nfs::Message message);
};
class DataHolder {
 public:
  DataHolder();
  virtual ~DataHolder();
  virtual void HandleMessage(nfs::Message message);
};

// TODO(Alison) - move mocks to separate file?
class MockMaidAccountHolder : public MaidAccountHolder {
 public:
  MockMaidAccountHolder();
  virtual ~MockMaidAccountHolder();

  MOCK_METHOD1(HandleMessage, void(nfs::Message message));

 private:
  MockMaidAccountHolder &operator=(const MockMaidAccountHolder&);
  MockMaidAccountHolder(const MockMaidAccountHolder&);
};

class MockMetadataManager : public MetadataManager {
 public:
  MockMetadataManager();
  virtual ~MockMetadataManager();

  MOCK_METHOD1(HandleMessage, void(nfs::Message message));

 private:
  MockMetadataManager &operator=(const MockMetadataManager&);
  MockMetadataManager(const MockMetadataManager&);
};

class MockPmidAccountHolder : public PmidAccountHolder {
 public:
  MockPmidAccountHolder();
  virtual ~MockPmidAccountHolder();

  MOCK_METHOD1(HandleMessage, void(nfs::Message message));

 private:
  MockPmidAccountHolder &operator=(const MockPmidAccountHolder&);
  MockPmidAccountHolder(const MockPmidAccountHolder&);
};

class MockDataHolder : public DataHolder {
 public:
  MockDataHolder();
  virtual ~MockDataHolder();

  MOCK_METHOD1(HandleMessage, void(nfs::Message message));

 private:
  MockDataHolder &operator=(const MockDataHolder&);
  MockDataHolder(const MockDataHolder&);
};

namespace test {


class DemultiplexerTest : public testing::Test {
 public:
  DemultiplexerTest()
      : maid_account_holder_(),
        meta_data_manager_(),
        pmid_account_holder_(),
        data_holder_(),
        demultiplexer_(maid_account_holder_,
                       meta_data_manager_,
                       pmid_account_holder_,
                       data_holder_) {}

  bool VerifyAndClearAllExpectations() {
    return testing::Mock::VerifyAndClearExpectations(&maid_account_holder_) &&
           testing::Mock::VerifyAndClearExpectations(&meta_data_manager_) &&
           testing::Mock::VerifyAndClearExpectations(&pmid_account_holder_) &&
           testing::Mock::VerifyAndClearExpectations(&data_holder_);
  }

  nfs::Message GenerateValidMessage(const nfs::PersonaType& type) {
    // TODO(Alison) - % 4 to match kActionType enum - improve?
    // TODO(Alison) - % 3 to match kDataType enum - improve?
    nfs::Message message(static_cast<nfs::ActionType>(RandomUint32() % 4),
                         type,
                         RandomUint32() % 3,
                         NodeId(NodeId::kRandomId),
                         NodeId(NodeId::kRandomId),
                         RandomAlphaNumericString(10),
                         RandomAlphaNumericString(10));
    return message;
  }

  nfs::Message GenerateValidMessage(uint16_t& expect_mah,
                                    uint16_t& expect_mdm,
                                    uint16_t& expect_pah,
                                    uint16_t& expect_dh) {
    // TODO(Alison) - % 4 to match PersonaType enum - improve?
    nfs::PersonaType type(static_cast<nfs::PersonaType>(RandomUint32() % 4));
    switch (type) {
      case nfs::PersonaType::kMaidAccountHolder:
        ++expect_mah;
        break;
      case nfs::PersonaType::kMetaDataManager:
        ++expect_mdm;
        break;
      case nfs::PersonaType::kPmidAccountHolder:
        ++expect_pah;
        break;
      case nfs::PersonaType::kDataHolder:
        ++expect_dh;
        break;
      default:
        LOG(kError) << "This type of message shouldn't occur here!";
        assert(false);
        break;
    }
    return GenerateValidMessage(type);
  }

  std::vector<nfs::Message> GenerateValidMessages(const uint16_t& num_messages,
                                                  uint16_t& expect_mah,
                                                  uint16_t& expect_mdm,
                                                  uint16_t& expect_pah,
                                                  uint16_t& expect_dh) {
    std::vector<nfs::Message> messages;
    if (num_messages == 0) {
      LOG(kError) << "Generated 0 messages.";
      return messages;
    }
    for (uint16_t i(0); i < num_messages; ++i) {
      messages.push_back(GenerateValidMessage(expect_mah, expect_mdm, expect_pah, expect_dh));
    }
    return messages;
  }

 protected:
  virtual void SetUp();

  virtual void TearDown() {
    EXPECT_TRUE(VerifyAndClearAllExpectations());
  }

 public:
  MockMaidAccountHolder maid_account_holder_;
  MockMetadataManager meta_data_manager_;
  MockPmidAccountHolder pmid_account_holder_;
  MockDataHolder data_holder_;
  Demultiplexer demultiplexer_;
};

TEST_F(DemultiplexerTest, FUNC_MaidAccountHolder) {
  nfs::Message message(GenerateValidMessage(nfs::PersonaType::kMaidAccountHolder));

  EXPECT_CALL(maid_account_holder_, HandleMessage(message)).Times(1);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  demultiplexer_.HandleMessage(SerialiseAsString(message));
}

TEST_F(DemultiplexerTest, FUNC_MaidAccountHolderRepeat) {
  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(100);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  for (uint16_t i(0); i < 100; ++i) {
    nfs::Message message(GenerateValidMessage(nfs::PersonaType::kMaidAccountHolder));
    demultiplexer_.HandleMessage(SerialiseAsString(message));
  }
}

TEST_F(DemultiplexerTest, FUNC_MetaDataManager) {
  nfs::Message message(GenerateValidMessage(nfs::PersonaType::kMetaDataManager));

  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(message)).Times(1);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  demultiplexer_.HandleMessage(SerialiseAsString(message));
}

TEST_F(DemultiplexerTest, FUNC_MetaDataManagerRepeat) {
  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(100);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  for (uint16_t i(0); i < 100; ++i) {
    nfs::Message message(GenerateValidMessage(nfs::PersonaType::kMetaDataManager));
    demultiplexer_.HandleMessage(SerialiseAsString(message));
  }
}

TEST_F(DemultiplexerTest, FUNC_PmidAccountHolder) {
  nfs::Message message(GenerateValidMessage(nfs::PersonaType::kPmidAccountHolder));

  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(message)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(1);

  demultiplexer_.HandleMessage(SerialiseAsString(message));
}

TEST_F(DemultiplexerTest, FUNC_PmidAccountHolderRepeat) {
  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(100);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  for (uint16_t i(0); i < 100; ++i) {
    nfs::Message message(GenerateValidMessage(nfs::PersonaType::kPmidAccountHolder));
    demultiplexer_.HandleMessage(SerialiseAsString(message));
  }
}

TEST_F(DemultiplexerTest, FUNC_DataHolder) {
  nfs::Message message(GenerateValidMessage(nfs::PersonaType::kDataHolder));

  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(message)).Times(1);

  demultiplexer_.HandleMessage(SerialiseAsString(message));
}

TEST_F(DemultiplexerTest, FUNC_DataHolderRepeat) {
  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(100);

  for (uint16_t i(0); i < 100; ++i) {
    nfs::Message message(GenerateValidMessage(nfs::PersonaType::kDataHolder));
    demultiplexer_.HandleMessage(SerialiseAsString(message));
  }
}

TEST_F(DemultiplexerTest, FUNC_Scrambled) {
  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  demultiplexer_.HandleMessage(RandomAlphaNumericString(1 + (RandomUint32() % 100)));
}

TEST_F(DemultiplexerTest, FUNC_ScrambledRepeat) {
  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  for (uint16_t i(0); i < 100; ++i) {
    std::string scrambled_message(RandomAlphaNumericString(1 + (RandomUint32() % 100)));
    demultiplexer_.HandleMessage(scrambled_message);
  }
}

TEST_F(DemultiplexerTest, FUNC_EmptyMessage) {
  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

  std::string empty_message;
  demultiplexer_.HandleMessage(empty_message);
}

TEST_F(DemultiplexerTest, FUNC_ValidMessages) {
  std::vector<nfs::Message> messages;
  messages.push_back(GenerateValidMessage(nfs::PersonaType::kMaidAccountHolder));
  messages.push_back(GenerateValidMessage(nfs::PersonaType::kMetaDataManager));
  messages.push_back(GenerateValidMessage(nfs::PersonaType::kPmidAccountHolder));
  messages.push_back(GenerateValidMessage(nfs::PersonaType::kDataHolder));

  while (messages.size() > 0) {
    uint16_t index(0);
    if (messages.size() > 1)
      index = RandomUint32() % messages.size();

    if (messages.at(index).persona_type() == nfs::PersonaType::kMaidAccountHolder)
      EXPECT_CALL(maid_account_holder_, HandleMessage(messages.at(index))).Times(1);
    else
      EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(0);

    if (messages.at(index).persona_type() == nfs::PersonaType::kMetaDataManager)
      EXPECT_CALL(meta_data_manager_, HandleMessage(messages.at(index))).Times(1);
    else
      EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(0);

    if (messages.at(index).persona_type() == nfs::PersonaType::kPmidAccountHolder)
      EXPECT_CALL(pmid_account_holder_, HandleMessage(messages.at(index))).Times(1);
    else
      EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(0);

    if (messages.at(index).persona_type() == nfs::PersonaType::kDataHolder)
      EXPECT_CALL(data_holder_, HandleMessage(messages.at(index))).Times(1);
    else
      EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(0);

    demultiplexer_.HandleMessage(SerialiseAsString(messages.at(index)));
    EXPECT_TRUE(VerifyAndClearAllExpectations());
    messages.erase(messages.begin() + index);
  }
}

TEST_F(DemultiplexerTest, FUNC_ValidMessagesRepeat) {
  uint16_t expect_mah(0),
           expect_mdm(0),
           expect_pah(0),
           expect_dh(0);
  std::vector<nfs::Message> messages(GenerateValidMessages(100,
                                                           expect_mah,
                                                           expect_mdm,
                                                           expect_pah,
                                                           expect_dh));

  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(expect_mah);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(expect_mdm);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(expect_pah);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(expect_dh);

  for (auto message : messages)
    demultiplexer_.HandleMessage(SerialiseAsString(message));
}

TEST_F(DemultiplexerTest, FUNC_ValidMessagesParallel) {
  uint16_t num_messages = 4 + (RandomUint32() % 7);
  uint16_t expect_mah(0),
           expect_mdm(0),
           expect_pah(0),
           expect_dh(0);

  std::vector<nfs::Message> messages(GenerateValidMessages(num_messages,
                                                           expect_mah,
                                                           expect_mdm,
                                                           expect_pah,
                                                           expect_dh));

  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(expect_mah);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(expect_mdm);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(expect_pah);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(expect_dh);

  std::vector<boost::thread> threads(num_messages);
  for (uint16_t i(0); i < num_messages; ++i) {
    threads[i] = boost::thread([&] {
                               demultiplexer_.HandleMessage(SerialiseAsString(messages[i]));
                 });
  }
  for (boost::thread& thread : threads)
    thread.join();
}

TEST_F(DemultiplexerTest, FUNC_MixedMessagesRepeat) {
  uint16_t num_messages(100);
  uint16_t expect_mah(0),
           expect_mdm(0),
           expect_pah(0),
           expect_dh(0);

  std::vector<nfs::Message> messages(GenerateValidMessages(num_messages,
                                                           expect_mah,
                                                           expect_mdm,
                                                           expect_pah,
                                                           expect_dh));

  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(expect_mah);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(expect_mdm);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(expect_pah);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(expect_dh);

  for (uint16_t i(0); i < num_messages; ++i) {
    if (i % 13 == 0 || i % 19 == 0) {
      std::string bad_message(RandomAlphaNumericString(1 + RandomUint32() % 50));
      demultiplexer_.HandleMessage(bad_message);
    }
    demultiplexer_.HandleMessage(SerialiseAsString(messages.at(i)));
  }
}

TEST_F(DemultiplexerTest, FUNC_MixedMessagesParallel) {
  uint16_t num_messages = 4 + (RandomUint32() % 7);
  uint16_t expect_mah(0),
           expect_mdm(0),
           expect_pah(0),
           expect_dh(0);

  std::vector<nfs::Message> messages(GenerateValidMessages(num_messages,
                                                           expect_mah,
                                                           expect_mdm,
                                                           expect_pah,
                                                           expect_dh));

  EXPECT_CALL(maid_account_holder_, HandleMessage(testing::_)).Times(expect_mah);
  EXPECT_CALL(meta_data_manager_, HandleMessage(testing::_)).Times(expect_mdm);
  EXPECT_CALL(pmid_account_holder_, HandleMessage(testing::_)).Times(expect_pah);
  EXPECT_CALL(data_holder_, HandleMessage(testing::_)).Times(expect_dh);

  std::vector<boost::thread> threads(num_messages);
  std::vector<boost::thread> bad_threads;
  for (uint16_t i(0); i < num_messages; ++i) {
    if (i % 13 == 0 || 1 % 19 == 0) {
      std::string bad_message(RandomAlphaNumericString(1 + RandomUint32() % 50));
      bad_threads.push_back(boost::thread([&] {
                              demultiplexer_.HandleMessage(bad_message);
                            }));
    }
    threads[i] = boost::thread([&] {
                               demultiplexer_.HandleMessage(nfs::SerialiseAsString(messages[i]));
                 });
  }
  for (boost::thread& thread : bad_threads)
    thread.join();
  for (boost::thread& thread : threads)
    thread.join();
}

// TODO(Alison) - add tests for caching

}  // namespace test

}  // namespace vault

}  // namespace maidsafe
