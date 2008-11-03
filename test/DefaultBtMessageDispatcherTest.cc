#include "DefaultBtMessageDispatcher.h"

#include <cassert>

#include <cppunit/extensions/HelperMacros.h>

#include "Util.h"
#include "Exception.h"
#include "MockPieceStorage.h"
#include "MockPeerStorage.h"
#include "DefaultBtContext.h"
#include "MockBtMessage.h"
#include "MockBtMessageFactory.h"
#include "prefs.h"
#include "BtCancelSendingPieceEvent.h"
#include "BtHandshakeMessage.h"

namespace aria2 {

class DefaultBtMessageDispatcherTest:public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DefaultBtMessageDispatcherTest);
  CPPUNIT_TEST(testAddMessage);
  CPPUNIT_TEST(testSendMessages);
  CPPUNIT_TEST(testSendMessages_underUploadLimit);
  CPPUNIT_TEST(testSendMessages_overUploadLimit);
  CPPUNIT_TEST(testSendMessages_sendingInProgress);
  CPPUNIT_TEST(testDoCancelSendingPieceAction);
  CPPUNIT_TEST(testCheckRequestSlotAndDoNecessaryThing);
  CPPUNIT_TEST(testCheckRequestSlotAndDoNecessaryThing_timeout);
  CPPUNIT_TEST(testCheckRequestSlotAndDoNecessaryThing_completeBlock);
  CPPUNIT_TEST(testIsSendingInProgress);
  CPPUNIT_TEST(testCountOutstandingRequest);
  CPPUNIT_TEST(testIsOutstandingRequest);
  CPPUNIT_TEST(testGetOutstandingRequest);
  CPPUNIT_TEST(testRemoveOutstandingRequest);
  CPPUNIT_TEST_SUITE_END();
private:
  SharedHandle<BtContext> btContext;
  SharedHandle<Peer> peer;
  SharedHandle<DefaultBtMessageDispatcher> btMessageDispatcher;
  SharedHandle<MockPeerStorage> peerStorage;
  SharedHandle<MockPieceStorage> pieceStorage;
  SharedHandle<MockBtMessageFactory> _messageFactory;
public:
  void tearDown() {}

  void testAddMessage();
  void testSendMessages();
  void testSendMessages_underUploadLimit();
  void testSendMessages_overUploadLimit();
  void testSendMessages_sendingInProgress();
  void testDoCancelSendingPieceAction();
  void testCheckRequestSlotAndDoNecessaryThing();
  void testCheckRequestSlotAndDoNecessaryThing_timeout();
  void testCheckRequestSlotAndDoNecessaryThing_completeBlock();
  void testIsSendingInProgress();
  void testCountOutstandingRequest();
  void testIsOutstandingRequest();
  void testGetOutstandingRequest();
  void testRemoveOutstandingRequest();

  class MockBtMessage2 : public MockBtMessage {
  private:
    bool onQueuedCalled;
    bool sendCalled;
    bool doCancelActionCalled;
  public:
    std::string type;
  public:
    MockBtMessage2():onQueuedCalled(false),
		     sendCalled(false),
		     doCancelActionCalled(false)
    {}

    virtual ~MockBtMessage2() {}

    virtual void onQueued() {
      onQueuedCalled = true;
    }

    bool isOnQueuedCalled() const {
      return onQueuedCalled;
    }

    virtual void send() {
      sendCalled = true;
    }

    bool isSendCalled() const {
      return sendCalled;
    }

    virtual void handleEvent(const SharedHandle<BtEvent>& event) {
      BtCancelSendingPieceEvent* e =
	dynamic_cast<BtCancelSendingPieceEvent*>(event.get());
      if(e) {
	doCancelActionCalled = true;
      }
    }

    bool isDoCancelActionCalled() const {
      return doCancelActionCalled;
    }  
  };

  class MockPieceStorage2 : public MockPieceStorage {
  private:
    SharedHandle<Piece> piece;
  public:
    virtual SharedHandle<Piece> getPiece(size_t index) {
      return piece;
    }

    void setPiece(const SharedHandle<Piece>& piece) {
      this->piece = piece;
    }
  };

  class MockBtMessageFactory2 : public MockBtMessageFactory {
  public:
    virtual SharedHandle<BtMessage>
    createCancelMessage(size_t index, uint32_t begin, size_t length) {
      SharedHandle<MockBtMessage2> btMsg(new MockBtMessage2());
      btMsg->type = "cancel";
      return btMsg;
    }
  };

  void setUp() {
    btContext.reset(new DefaultBtContext());
    btContext->load("test.torrent");
    peer.reset(new Peer("192.168.0.1", 6969));
    peer->allocateSessionResource(btContext->getPieceLength(),
				  btContext->getTotalLength());
    peerStorage.reset(new MockPeerStorage());
    pieceStorage.reset(new MockPieceStorage());

    _messageFactory.reset(new MockBtMessageFactory2());

    btMessageDispatcher.reset(new DefaultBtMessageDispatcher());
    btMessageDispatcher->setPeer(peer);
    btMessageDispatcher->setBtContext(btContext);
    btMessageDispatcher->setPieceStorage(pieceStorage);
    btMessageDispatcher->setPeerStorage(peerStorage);
    btMessageDispatcher->setBtMessageFactory(_messageFactory);
    btMessageDispatcher->setCuid(1);
    btMessageDispatcher->setMaxUploadSpeedLimit(0);
  }
};


CPPUNIT_TEST_SUITE_REGISTRATION(DefaultBtMessageDispatcherTest);

void DefaultBtMessageDispatcherTest::testAddMessage() {
  SharedHandle<MockBtMessage2> msg(new MockBtMessage2());
  CPPUNIT_ASSERT_EQUAL(false, msg->isOnQueuedCalled());
  btMessageDispatcher->addMessageToQueue(msg);
  CPPUNIT_ASSERT_EQUAL(true, msg->isOnQueuedCalled());
  CPPUNIT_ASSERT_EQUAL((size_t)1,
		       btMessageDispatcher->getMessageQueue().size());
}

void DefaultBtMessageDispatcherTest::testSendMessages() {
  TransferStat stat;
  stat.setUploadSpeed(0);
  peerStorage->setStat(stat);

  SharedHandle<MockBtMessage2> msg1(new MockBtMessage2());
  msg1->setSendingInProgress(false);
  msg1->setUploading(false);
  SharedHandle<MockBtMessage2> msg2(new MockBtMessage2());
  msg2->setSendingInProgress(false);
  msg2->setUploading(false);
  btMessageDispatcher->addMessageToQueue(msg1);
  btMessageDispatcher->addMessageToQueue(msg2);
  btMessageDispatcher->sendMessages();

  CPPUNIT_ASSERT(msg1->isSendCalled());
  CPPUNIT_ASSERT(msg2->isSendCalled());
}

void DefaultBtMessageDispatcherTest::testSendMessages_underUploadLimit() {
  TransferStat stat;
  stat.setUploadSpeed(0);
  peerStorage->setStat(stat);

  SharedHandle<MockBtMessage2> msg1(new MockBtMessage2());
  msg1->setSendingInProgress(false);
  msg1->setUploading(true);
  SharedHandle<MockBtMessage2> msg2(new MockBtMessage2());
  msg2->setSendingInProgress(false);
  msg2->setUploading(true);
  btMessageDispatcher->addMessageToQueue(msg1);
  btMessageDispatcher->addMessageToQueue(msg2);
  btMessageDispatcher->sendMessages();

  CPPUNIT_ASSERT(msg1->isSendCalled());
  CPPUNIT_ASSERT(msg2->isSendCalled());
}

void DefaultBtMessageDispatcherTest::testSendMessages_overUploadLimit() {
  btMessageDispatcher->setMaxUploadSpeedLimit(100);
  TransferStat stat;
  stat.setUploadSpeed(150);
  peerStorage->setStat(stat);

  SharedHandle<MockBtMessage2> msg1(new MockBtMessage2());
  msg1->setSendingInProgress(false);
  msg1->setUploading(true);
  SharedHandle<MockBtMessage2> msg2(new MockBtMessage2());
  msg2->setSendingInProgress(false);
  msg2->setUploading(true);
  SharedHandle<MockBtMessage2> msg3(new MockBtMessage2());
  msg3->setSendingInProgress(false);
  msg3->setUploading(false);

  btMessageDispatcher->addMessageToQueue(msg1);
  btMessageDispatcher->addMessageToQueue(msg2);
  btMessageDispatcher->addMessageToQueue(msg3);
  btMessageDispatcher->sendMessages();

  CPPUNIT_ASSERT(!msg1->isSendCalled());
  CPPUNIT_ASSERT(!msg2->isSendCalled());
  CPPUNIT_ASSERT(msg3->isSendCalled());

  CPPUNIT_ASSERT_EQUAL((size_t)2,
		       btMessageDispatcher->getMessageQueue().size());
}

void DefaultBtMessageDispatcherTest::testSendMessages_sendingInProgress() {
  SharedHandle<MockBtMessage2> msg1(new MockBtMessage2());
  msg1->setSendingInProgress(false);
  msg1->setUploading(false);
  SharedHandle<MockBtMessage2> msg2(new MockBtMessage2());
  msg2->setSendingInProgress(true);
  msg2->setUploading(false);
  SharedHandle<MockBtMessage2> msg3(new MockBtMessage2());
  msg3->setSendingInProgress(false);
  msg3->setUploading(false);

  btMessageDispatcher->addMessageToQueue(msg1);
  btMessageDispatcher->addMessageToQueue(msg2);
  btMessageDispatcher->addMessageToQueue(msg3);

  btMessageDispatcher->sendMessages();

  CPPUNIT_ASSERT(msg1->isSendCalled());
  CPPUNIT_ASSERT(msg2->isSendCalled());
  CPPUNIT_ASSERT(!msg3->isSendCalled());

  CPPUNIT_ASSERT_EQUAL((size_t)2,
		       btMessageDispatcher->getMessageQueue().size());
}

void DefaultBtMessageDispatcherTest::testDoCancelSendingPieceAction() {
  SharedHandle<MockBtMessage2> msg1(new MockBtMessage2());
  SharedHandle<MockBtMessage2> msg2(new MockBtMessage2());

  btMessageDispatcher->addMessageToQueue(msg1);
  btMessageDispatcher->addMessageToQueue(msg2);

  btMessageDispatcher->doCancelSendingPieceAction(0, 0, 0);

  CPPUNIT_ASSERT_EQUAL(true, msg1->isDoCancelActionCalled());
  CPPUNIT_ASSERT_EQUAL(true, msg2->isDoCancelActionCalled());
}

int MY_PIECE_LENGTH = 16*1024;

void DefaultBtMessageDispatcherTest::testCheckRequestSlotAndDoNecessaryThing() {
  SharedHandle<Piece> piece(new Piece(0, MY_PIECE_LENGTH));
  RequestSlot slot(0, 0, MY_PIECE_LENGTH, 0, piece);
  
  size_t index;
  CPPUNIT_ASSERT(piece->getMissingUnusedBlockIndex(index));
  CPPUNIT_ASSERT_EQUAL((size_t)0, index);

  SharedHandle<MockPieceStorage2> pieceStorage(new MockPieceStorage2());
  pieceStorage->setPiece(piece);
  
  btMessageDispatcher->setRequestTimeout(60);
  btMessageDispatcher->setPieceStorage(pieceStorage);
  btMessageDispatcher->addOutstandingRequest(slot);

  btMessageDispatcher->checkRequestSlotAndDoNecessaryThing();

  CPPUNIT_ASSERT_EQUAL((size_t)0,
		       btMessageDispatcher->getMessageQueue().size());
  CPPUNIT_ASSERT_EQUAL((size_t)1,
		       btMessageDispatcher->getRequestSlots().size());
}

void DefaultBtMessageDispatcherTest::testCheckRequestSlotAndDoNecessaryThing_timeout() {
  SharedHandle<Piece> piece(new Piece(0, MY_PIECE_LENGTH));
  RequestSlot slot(0, 0, MY_PIECE_LENGTH, 0, piece);
  // make this slot timeout
  slot.setDispatchedTime(0);

  size_t index;
  CPPUNIT_ASSERT(piece->getMissingUnusedBlockIndex(index));
  CPPUNIT_ASSERT_EQUAL((size_t)0, index);

  SharedHandle<MockPieceStorage2> pieceStorage(new MockPieceStorage2());
  pieceStorage->setPiece(piece);

  btMessageDispatcher->setRequestTimeout(60);
  btMessageDispatcher->setPieceStorage(pieceStorage);
  btMessageDispatcher->addOutstandingRequest(slot);

  btMessageDispatcher->checkRequestSlotAndDoNecessaryThing();

  CPPUNIT_ASSERT_EQUAL((size_t)0,
		       btMessageDispatcher->getMessageQueue().size());
  CPPUNIT_ASSERT_EQUAL((size_t)0,
		       btMessageDispatcher->getRequestSlots().size());
  CPPUNIT_ASSERT_EQUAL(false, piece->isBlockUsed(0));
  CPPUNIT_ASSERT_EQUAL(true, peer->snubbing());
}

void DefaultBtMessageDispatcherTest::testCheckRequestSlotAndDoNecessaryThing_completeBlock() {
  SharedHandle<Piece> piece(new Piece(0, MY_PIECE_LENGTH));
  piece->completeBlock(0);

  RequestSlot slot(0, 0, MY_PIECE_LENGTH, 0, piece);
  
  SharedHandle<MockPieceStorage2> pieceStorage(new MockPieceStorage2());
  pieceStorage->setPiece(piece);

  btMessageDispatcher->setRequestTimeout(60);
  btMessageDispatcher->setPieceStorage(pieceStorage);
  btMessageDispatcher->addOutstandingRequest(slot);

  btMessageDispatcher->checkRequestSlotAndDoNecessaryThing();

  CPPUNIT_ASSERT_EQUAL((size_t)1,
		       btMessageDispatcher->getMessageQueue().size());
  CPPUNIT_ASSERT_EQUAL((size_t)0,
		       btMessageDispatcher->getRequestSlots().size());
}

void DefaultBtMessageDispatcherTest::testIsSendingInProgress() {
  CPPUNIT_ASSERT(!btMessageDispatcher->isSendingInProgress());
  SharedHandle<MockBtMessage2> msg(new MockBtMessage2());
  msg->setSendingInProgress(false);
  btMessageDispatcher->addMessageToQueue(msg);
  CPPUNIT_ASSERT(!btMessageDispatcher->isSendingInProgress());
  msg->setSendingInProgress(true);
  CPPUNIT_ASSERT(btMessageDispatcher->isSendingInProgress());
}

void DefaultBtMessageDispatcherTest::testCountOutstandingRequest() {
  RequestSlot slot(0, 0, MY_PIECE_LENGTH, 0);
  btMessageDispatcher->addOutstandingRequest(slot);
  CPPUNIT_ASSERT_EQUAL((size_t)1,
		       btMessageDispatcher->countOutstandingRequest());
}

void DefaultBtMessageDispatcherTest::testIsOutstandingRequest() {
  RequestSlot slot(0, 0, MY_PIECE_LENGTH, 0);
  btMessageDispatcher->addOutstandingRequest(slot);

  CPPUNIT_ASSERT(btMessageDispatcher->isOutstandingRequest(0, 0));
  CPPUNIT_ASSERT(!btMessageDispatcher->isOutstandingRequest(0, 1));
  CPPUNIT_ASSERT(!btMessageDispatcher->isOutstandingRequest(1, 0));  
  CPPUNIT_ASSERT(!btMessageDispatcher->isOutstandingRequest(1, 1));  
}

void DefaultBtMessageDispatcherTest::testGetOutstandingRequest() {
  RequestSlot slot(1, 1024, 16*1024, 10);
  btMessageDispatcher->addOutstandingRequest(slot);

  RequestSlot s2 = btMessageDispatcher->getOutstandingRequest(1, 1024, 16*1024);
  CPPUNIT_ASSERT(!RequestSlot::isNull(s2));

  RequestSlot s3 = btMessageDispatcher->getOutstandingRequest(1, 1024, 17*1024);
  CPPUNIT_ASSERT(RequestSlot::isNull(s3));

  RequestSlot s4 =
    btMessageDispatcher->getOutstandingRequest(1, 2*1024, 16*1024);
  CPPUNIT_ASSERT(RequestSlot::isNull(s4));

  RequestSlot s5 = btMessageDispatcher->getOutstandingRequest(2, 1024, 16*1024);
  CPPUNIT_ASSERT(RequestSlot::isNull(s5));
}

void DefaultBtMessageDispatcherTest::testRemoveOutstandingRequest() {
  RequestSlot slot(1, 1024, 16*1024, 10);
  btMessageDispatcher->addOutstandingRequest(slot);

  RequestSlot s2 = btMessageDispatcher->getOutstandingRequest(1, 1024, 16*1024);
  CPPUNIT_ASSERT(!RequestSlot::isNull(s2));

  btMessageDispatcher->removeOutstandingRequest(s2);

  RequestSlot s3 = btMessageDispatcher->getOutstandingRequest(1, 1024, 16*1024);
  CPPUNIT_ASSERT(RequestSlot::isNull(s3));  
}

} // namespace aria2
