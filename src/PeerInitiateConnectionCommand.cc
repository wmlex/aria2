/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "PeerInitiateConnectionCommand.h"
#include "InitiatorMSEHandshakeCommand.h"
#include "PeerInteractionCommand.h"
#include "DownloadEngine.h"
#include "DlAbortEx.h"
#include "message.h"
#include "prefs.h"
#include "CUIDCounter.h"
#include "Socket.h"
#include "Logger.h"
#include "Peer.h"
#include "BtContext.h"
#include "BtRuntime.h"
#include "PeerStorage.h"
#include "PieceStorage.h"
#include "PeerConnection.h"

namespace aria2 {

PeerInitiateConnectionCommand::PeerInitiateConnectionCommand
(int cuid,
 RequestGroup* requestGroup,
 const PeerHandle& peer,
 DownloadEngine* e,
 const SharedHandle<BtContext>& btContext,
 const SharedHandle<BtRuntime>& btRuntime,
 bool mseHandshakeEnabled)
  :
  PeerAbstractCommand(cuid, peer, e),
  RequestGroupAware(requestGroup),
  _btContext(btContext),
  _btRuntime(btRuntime),
  _mseHandshakeEnabled(mseHandshakeEnabled)
{
  _btRuntime->increaseConnections();
}

PeerInitiateConnectionCommand::~PeerInitiateConnectionCommand()
{
  _btRuntime->decreaseConnections();
}

bool PeerInitiateConnectionCommand::executeInternal() {
  logger->info(MSG_CONNECTING_TO_SERVER, cuid, peer->ipaddr.c_str(),
	       peer->port);
  socket.reset(new SocketCore());
  socket->establishConnection(peer->ipaddr, peer->port);
  if(_mseHandshakeEnabled) {
    InitiatorMSEHandshakeCommand* c =
      new InitiatorMSEHandshakeCommand(cuid, _requestGroup, peer, e, _btContext,
				       _btRuntime, socket);
    c->setPeerStorage(_peerStorage);
    c->setPieceStorage(_pieceStorage);
    e->commands.push_back(c);
  } else {
    PeerInteractionCommand* command =
      new PeerInteractionCommand
      (cuid, _requestGroup, peer, e, _btContext, _btRuntime, _pieceStorage,
       socket, PeerInteractionCommand::INITIATOR_SEND_HANDSHAKE);
    command->setPeerStorage(_peerStorage);
    e->commands.push_back(command);
  }
  return true;
}

// TODO this method removed when PeerBalancerCommand is implemented
bool PeerInitiateConnectionCommand::prepareForNextPeer(time_t wait) {
  if(_peerStorage->isPeerAvailable() && _btRuntime->lessThanEqMinPeers()) {
    PeerHandle peer = _peerStorage->getUnusedPeer();
    peer->usedBy(CUIDCounterSingletonHolder::instance()->newID());
    PeerInitiateConnectionCommand* command =
      new PeerInitiateConnectionCommand(peer->usedBy(), _requestGroup, peer, e,
					_btContext, _btRuntime);
    command->setPeerStorage(_peerStorage);
    command->setPieceStorage(_pieceStorage);
    e->commands.push_back(command);
  }
  return true;
}

void PeerInitiateConnectionCommand::onAbort() {
  _peerStorage->returnPeer(peer);
}

bool PeerInitiateConnectionCommand::exitBeforeExecute()
{
  return _btRuntime->isHalt();
}

void PeerInitiateConnectionCommand::setPeerStorage
(const SharedHandle<PeerStorage>& peerStorage)
{
  _peerStorage = peerStorage;
}

void PeerInitiateConnectionCommand::setPieceStorage
(const SharedHandle<PieceStorage>& pieceStorage)
{
  _pieceStorage = pieceStorage;
}

} // namespace aria2
