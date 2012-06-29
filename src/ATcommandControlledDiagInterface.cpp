/*
 * ATcommandControlledDiagInterface.cpp - AT-command controlled diagnostic interface
 *
 * Copyright (C) 2012 Comer352L
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ATcommandControlledDiagInterface.h"

#ifdef __WIN32__
    #define waitms(x) Sleep(x)
#elif defined __linux__
    #define waitms(x) usleep(1000*x)
#else
    #error "Operating system not supported !"
#endif


ATcommandControlledDiagInterface::ATcommandControlledDiagInterface()
{
	_port = NULL;
	_connected = false;
	_baudrate = 0;
	_custom_baudrate = false;
	_if_model = if_none;
	_try_echo_detection = true;
	_linefeed_enabled = false;
	_headers_enabled = false;
	_target_addr = 0x7E0u;
	_flush_local_Rx_buffer = false;
	_ready = false;
	_exit = false;
	setName("AT-command controlled");
	setVersion("");
}


ATcommandControlledDiagInterface::~ATcommandControlledDiagInterface()
{
	if (isOpen())
		close();
}


AbstractDiagInterface::interface_type ATcommandControlledDiagInterface::interfaceType()
{
	return interface_ATcommandControlled;
}


bool ATcommandControlledDiagInterface::open( std::string name )
{
	if (_port)
		return false;
	_port = new serialCOM;
	if (_port->OpenPort( name ))
	{
		if (!_port->SetControlLines(true, false))	// NOTE: ELM: enabling RTS means interrupting current operation
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::open():   warning: setting handshaking lines states failed ! This could prevent the interface from working !\n";
#endif
		}
		_ready = true;
		// Start read-thread;
		start();
		if (!isRunning())
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::open():   error: read-thread could not be started !\n";
#endif
			_port->ClosePort();
			delete _port;
			_port = NULL;
			return false;
		}
		// Probe baud rate / interface
		const unsigned int baudrates[3] = {115200, 38400, 9600}; // FIXME: change 115000 to 500000 (needs modifying serialCOM to support baud rates > 115200)
		bool ok = false;
		for (unsigned char bri=0; bri<3; bri++)
		{
			_mutex.lock();
			ok = _port->SetPortSettings(baudrates[bri], 8, 'N', 1);
			_flush_local_Rx_buffer = true;
			_mutex.unlock();
			if (ok)
			{
#ifdef __FSSM_DEBUG__
				std::cout << "ATcommandControlledDiagInterface::open():   probing interface at " << std::dec << baudrates[bri] << " baud...\n";
#endif
				_if_model = probeInterface();
				if (_if_model != if_none)
				{
					if (_if_model != if_unsupported)
						_baudrate = baudrates[bri];
					break;
				}
			}
#ifdef __FSSM_DEBUG__
			else
				std::cout << "ATcommandControlledDiagInterface::open():   error: failed to set serial port baud rate to " << std::dec << baudrates[bri] << " baud...\n";
#endif
		}
		// Configure interface
		if ((_if_model != if_none) && (_if_model != if_unsupported))
		{
			if (configureDevice())
				return true;
		}
		// Error path:
		_baudrate = 0;
		_if_model = if_unsupported;
		_mutex.lock();
		_exit = true;	// stop read-thread:
		_mutex.unlock();
		wait(1000);	// wait until read-thread finished, max. 1000ms
#ifdef __FSSM_DEBUG__
		if (isRunning())
			std::cout << "ATcommandControlledDiagInterface::open():   error: read-thread could not be stopped !\n";
#endif
	}
#ifdef __FSSM_DEBUG__
	else
		std::cout << "ATcommandControlledDiagInterface::open():   error: failed to open serial port !\n";
#endif
	delete _port;	// also closes port if it is open
	_port = NULL;
	return false;
}


bool ATcommandControlledDiagInterface::isOpen()
{
	return _port;
}


bool ATcommandControlledDiagInterface::close()
{
	if (_port)
	{
		// Disconnect if still connected:
		if (_connected)
			disconnect();
		// Reset interface
		if (_custom_baudrate)
			changeInterfaceBaudRate(38400);
		_custom_baudrate = false;
#ifdef __FSSM_DEBUG__
		if (writeRead("ATD") != "OK")
			std::cout << "ATcommandControlledDiagInterface::close():   error: failed to reset interface parameters !\n";
#else
		writeRead("ATD");
#endif
		// NOTE; we can not use writeRead("ATZ") if we are at a custom baud rate, because this will reset the baud rate and the reply is sent at the new baud rate
		// Stop read-thread:
		_mutex.lock();
		_exit = true;
		_mutex.unlock();
		wait(1000);	// wait until read-thread finished, max. 1000ms
#ifdef __FSSM_DEBUG__
		if (isRunning())
			std::cout << "ATcommandControlledDiagInterface::close():   error: read-thread is still running !\n";
#endif
		// Close port and clean up:
		if (_port->ClosePort())
		{
			delete _port;
			_port = NULL;
			_baudrate = 0;
			_if_model = if_none;
			_try_echo_detection = true;
			_target_addr = 0x7E0u;
			_flush_local_Rx_buffer = false;
			_lastcmd.clear();
			_RxQueue.clear();
			setName("AT-command controlled");
			setVersion("");
			return true;
		}
	}
	return false;
}


bool ATcommandControlledDiagInterface::connect(protocol_type protocol)
{
	if (_port && !_connected)
	{
		if ((_if_model == if_none) || (_if_model == if_unsupported))
			return false;
		if (protocol == AbstractDiagInterface::protocol_SSM2_ISO15765)
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::connect():   configuring interface for SSM2 via ISO-15765...\n";
#endif
			std::string reply;
			// Disable displaying message headers (CAN-ID + PCI-byte)	NOTE: processRecData() can handle both cases (on/off), but we need to know the current setting
			reply = writeRead("ATH0");
			if (reply != "OK")
			{
#ifdef __FSSM_DEBUG__
				std::cout << "ATcommandControlledDiagInterface::connect():   error: failed to disable message headers !\n";
#endif
				return false;
			}
			_headers_enabled = false;
			// Enable message padding
			if ((_if_model == if_model_AGV) || (_if_model == if_model_AGV4000B)) // AGV4000(B)+DX35+DX60/61
				reply = writeRead("ATCA1");	// Enable "CAN-Auto-format"
			else // ELM327+ELM329
				reply = writeRead("ATCAF1");	// Enable "CAN-Auto-format"
#ifdef __FSSM_DEBUG__
			if (reply != "OK")
				std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to enable CAN auto format (byte padding) !\n";
#endif
			// Enable messages with > 7 bytes
			if (_if_model == if_model_ELM327)
			{
				reply = writeRead("ATAL");	// since v1.2, this is not needed anymore at least for incoming messages
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to enable long size messages !\n";
#endif
			}
			// Disable extended CAN addressing
			if ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM329))	// others do not support extended addressing
			{
				reply = writeRead("ATCEA");	//  ELM327: since v1.4
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to disable extended CAN adressing !\n";
#endif
			}
			// Configure Rx/Tx addresses
			if (!changeCUAddresses(_target_addr))
				return false;
			// Configure flow control
			if ((_if_model == if_model_AGV) || (_if_model == if_model_AGV4000B)) // AGV4000(B)+AGV4500+DX35+DX60/61
			{
				reply = writeRead("ATCC1");	// Enable flow control messages
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   error: failed to enable flow control messages !\n";
#endif
				if (_if_model == if_model_AGV4000B)
				{
					reply = writeRead("ATCD0A");	// Set CAN flow delay
#ifdef __FSSM_DEBUG__
					if (reply != "OK")
						std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to set CAN flow delay !\n";
#endif
				}
			}
			else // ELM327+ELM329
			{
				reply = writeRead("ATCFC1");	// Enable flow control messages
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to enable flow control messages !\n";
#endif
				reply = writeRead("ATFCSM0");	// Set flow control mode to 0 "fully automatic responses" (parameters set with ATFCSH and ATFCSD are ignored)
				// NOTE: ELM327 supports ATFC commands since v1.1
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to set flow control mode to \"fully automatic responses\". This could cause communication problems !\n";
#endif
			}
			// Disable permanent wakeup messages
			reply = writeRead("ATSW00");	// AGV4000(B)+AGV4500+DX35+DX60/61: applies to ISO9141 and ISO14230 only; ELM329 (+ELM327 ?): also applies to CAN
#ifdef __FSSM_DEBUG__
			if (reply != "OK")
				std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to disable permanent wakeup messages !\n";
#endif
			// Configure timeout for incoming messages
			if ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM329))	// not supported by AGV, Diamex interfaces
			{
				reply = writeRead("ATR1");	// Enable displaying of incoming messages
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to enable displaying of incoming messages !\n";
#endif
				
				reply = writeRead("ATSTFA");	// Set maximum timeout value (0xFA=250 * 4ms=1000ms)
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to set maximum timeout value for message receiving !\n";
#endif
				reply = writeRead("ATAT1");	// Enable automatic adaptive timing control (level 1); ELM327: since v1.2
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to enable automatic adaptive timing control for receiving messges !\n";
#endif
			}
			// NOTE: AGV + Diamex interfaces do not provide a possibility to adjust the timings, they use the values described in the ISO norm
			// Set protocol ISO15765, 500kbps, 11bit-CAN-ID
			if ((_if_model == if_model_AGV) || (_if_model == if_model_AGV4000B))
				reply = writeRead("ATP6");	// AGV4000(B)+AGV4500+DX35+DX60/61
			else
				reply = writeRead("ATSP6");	// ELM327+ELM329
			if ((reply != "OK") &&
			    (!reply.size() || (reply.at(0) != '6') || (reply.find("15765") == std::string::npos) || (reply.find("11") == std::string::npos) || (reply.find("500") == std::string::npos))
			   )
			{
#ifdef __FSSM_DEBUG__
				std::cout << "ATcommandControlledDiagInterface::connect():   error: failed to set communication protocol to ISO15765 500kbps 8bit-ID !\n";
#endif
				return false;
			}
			// NOTE: AGV4000(B) returns "6 = ISO 15765-4, CAN (11/500)", ELM329 returns "OK"
			// Disable initialization	// NOTE: must be done AFTER AT(S)P
			if ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM329))
			{
				reply = writeRead("ATBI");
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to disable ISO15765-initialization-sequence !\n";
#endif
			}
			else if (_if_model == if_model_AGV4000B)
			{
				reply = writeRead("ATONI1");
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to disable ISO15765-initialization-sequence !\n";
#endif
			}
			// Disable skipping of sending data after connecting
			if (_if_model == if_model_AGV4000B)
			{
				reply = writeRead("ATONR0");	
#ifdef __FSSM_DEBUG__
				if (reply != "OK")
					std::cout << "ATcommandControlledDiagInterface::connect():   warning: failed to disable skipping of sending data after connecting !\n";
#endif
			}

			// NOTE: for ISO15765, there is no need to trigger a connect here, because there is no initialization sequence and no stay alive messages
		}
		else
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::connect():   the selected protocol is not supported.\n";
#endif
			return false;
		}
		setProtocolType( protocol );
		_connected = true;
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::connect():   completed.\n";
#endif
	}
	return _connected;
}


bool ATcommandControlledDiagInterface::isConnected()
{
	return (_port && _connected); 
}


bool ATcommandControlledDiagInterface::disconnect()
{
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::disconnect():   stopping communication.\n";
#endif
	if (_port)
	{
		// Stop communication
		if ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM329))
			writeRead("ATPC");	// ELM327/ELM329 only
		_connected = false;
		setProtocolType( protocol_NONE );
		return true;
	}
	return false;
}


bool ATcommandControlledDiagInterface::read(std::vector<char> *buffer)
{
	if (_port && _connected)
	{
		// Read reply message
		std::string msg;
		_mutex.lock();
		if (!_RxQueue.size())
		{
			_mutex.unlock();
			return true;	// no data available
		}
		msg = _RxQueue.at(0);
		_RxQueue.erase(_RxQueue.begin(), _RxQueue.begin()+1);
		_mutex.unlock();
		// Process received data
		*buffer = processRecData(msg);
		if (msg.size() && !buffer->size())
			return true;	// no data available
		// Prepend CAN-ID:
		char addr[4];
		addr[0] = (_target_addr & 0xff000000) >> 24;
		addr[1] = (_target_addr & 0xff0000) >> 16;
		addr[2] = (_target_addr & 0xff00) >> 8;
		addr[3] = (_target_addr & 0xff) + 8;
		// TODO: Extend address range
		buffer->insert(buffer->begin(), addr, addr+4);
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::read():";
		for (unsigned int c=0; c< buffer->size(); c++)
			std::cout << " " << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(buffer->at(c)));
		std::cout << "\n";
#endif
		return true;
	}
	return false;
}


bool ATcommandControlledDiagInterface::write(std::vector<char> buffer)
{
	if (_port && _connected)
	{
		if (buffer.size() <= 4)
			return false;
		// Check/Change CU address settings:
		unsigned int t_addr = static_cast<unsigned char>(buffer.at(0))*(0xffffff+1) + static_cast<unsigned char>(buffer.at(1))*(0xffff+1) + static_cast<unsigned char>(buffer.at(2))*(0xff+1) + static_cast<unsigned char>(buffer.at(3));
		if (t_addr != _target_addr)
		{
			if (!changeCUAddresses(t_addr))
				return false;
		}
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::write():";
		for (unsigned int c=0; c<buffer.size(); c++)
			std::cout << " " << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(buffer.at(c)));
		std::cout << "\n";
#endif
		// Strip CAN-ID (automatically prepended by interface):
		buffer.erase(buffer.begin(), buffer.begin() + 4);
		// Convert data to hex-string:
		std::string msg = dataToHexStr(buffer);
		buffer.assign(msg.begin(), msg.end());
		buffer.push_back('\r');
		// Send data:
		return transmitMessage(buffer, false);
	}
	return false;
}


bool ATcommandControlledDiagInterface::clearSendBuffer()
{
	_mutex.lock();
	_port->ClearSendBuffer();
	_mutex.unlock();
	return true;
}


bool ATcommandControlledDiagInterface::clearReceiveBuffer()
{
	_mutex.lock();
	_port->ClearReceiveBuffer();
	_RxQueue.clear();
	_flush_local_Rx_buffer = true;	// let read-thread flush local buffers before next read
	_mutex.unlock();
	return true;
}


// private


void ATcommandControlledDiagInterface::run()
{
	std::vector<char> tmp_buffer;
	std::string rec_buffer;
	bool exit = false;

	do
	{
		_mutex.lock();
		// Flush local receive buffer
		if (_flush_local_Rx_buffer)
		{
			rec_buffer.clear();
			_flush_local_Rx_buffer = false;
		}
		// Check if new data arrived
		unsigned int numbytes = 0;
		if (_port->GetNrOfBytesAvailable(&numbytes))
		{
			if (numbytes)
			{
				// Read all available bytes from device
				if (_port->Read( 0, numbytes, 0, &tmp_buffer ))
				{
					// Append the bytes read to received data queue:
					rec_buffer.append(tmp_buffer.begin(), tmp_buffer.end());
					// Extract all received messages
					size_t lend_pos = std::string::npos;
					size_t ready_pos = std::string::npos;
					bool lt_long = false;
					do
					{
						// Delete preceeding "ready" character(s)
						/* NOTE: there seems to be some rare cases where the ELM327 seems to send 2 ready characters ! */
						while (rec_buffer.size() && (rec_buffer.at(0) == '>'))
						{
							_ready = true;
							rec_buffer.erase(rec_buffer.begin(), rec_buffer.begin() + 1);
						}
						// Search for echo and eliminate:
						if (_try_echo_detection)
						{
							if (_lastcmd.size() && (_lastcmd.size() <= rec_buffer.size()))
							{
								if (rec_buffer.find(_lastcmd) == 0)
									rec_buffer.erase(rec_buffer.begin(), rec_buffer.begin() + _lastcmd.size());
								_lastcmd.clear();	// Disable echo detection for the following cycles
							}
							/* NOTE: Echo detection fails if 2 different commands are send in fast order without waiting for the reply
							 *       to the first command. But this is the mistake of the classes user and we only rely on this method if
							 *       the interface settings are unknow (until we have disabled the echo with ATE0 command).
							 */
						}
						// Elimiate preceeding CR and LF:
						/* NOTE: This is not only to delete CR and LF between echo and reply 
						   In some cases, the interface sends replies with preceeding CR (+LF) (e.g. ELM327: reply to command ATD) */
						while (rec_buffer.size() && ((rec_buffer.at(0) == '\r') || (rec_buffer.at(0) == '\n')))
							rec_buffer.erase(rec_buffer.begin(), rec_buffer.begin() + 1);
						/* Search for message termination and extract message */
						/* NOTE: Most interfaces terminate their messages with an empty line, but at least the AGV4000B doesn't.
						   That's why we also accept the "ready" character '>' as message termination.				 */
						/* TODO: How do we detect the message end when the control unit sends multiple messages in a row if we have an
						   interface that do not terminate its messages with an emtpy line ? (we can't use '>' as message end indicator in this case !) */
						/* NOTE: we could optimize this (start search at pos (byte 1 of new data - 3)), but the overhead seems to be not worth the benefit */
						// Search for empty line
						std::string msg;
						lend_pos = rec_buffer.find("\r\r");
						if (lend_pos != std::string::npos)
							lt_long = false;
						else
						{
							lend_pos = rec_buffer.find("\r\n\r\n");
							if (lend_pos != std::string::npos)
								lt_long = true;
						}
						// Search for "ready" character
						ready_pos = rec_buffer.find('>');
						if (ready_pos != std::string::npos)
						{
							_ready = true;
							if (ready_pos < lend_pos)
								lend_pos = ready_pos;
						}
						// Extract message
						/* NOTE: We don't extract empty messages !
						 * This would make things much more difficult and so far there is no known situation where an interface
						 * sends such a message (althought there is at least one case where the interface expects to receive an 
						 * empty message from us (ELM327/329 baud rate switching procedure) !)										 */
						if (lend_pos != std::string::npos)
						{
							msg.assign(rec_buffer.begin(), rec_buffer.begin()+lend_pos);
							// Delete message (including termination characters) from buffer
							if (lend_pos == ready_pos)	// message end with '>'
							{
								rec_buffer.erase(rec_buffer.begin(), rec_buffer.begin() + lend_pos + 1);
								// Delete line termination at the end of the message
								if (msg.size() && (msg.at(msg.size()-1) == '\r'))
								{
									msg.erase(msg.end()-1, msg.end());
								}
								else if ((msg.size() > 1) && ((msg.at(msg.size()-2) == '\r') && (msg.at(msg.size()-1) == '\n')))
								{
									msg.erase(msg.end()-2, msg.end());
								}
							}
							else				// normal message end with empty line
								rec_buffer.erase(rec_buffer.begin(), rec_buffer.begin() + lend_pos + 2+lt_long*2);
							// Append message to Rx-queue
							if (msg.size())
							{

								// TODO / FIXME: handle special messages, such as event messages (e.g. ACT ALERT, LP ALERT, LV RESET etc.)

								if ((_RxQueue.size() + 1) > 10)	// limit buffer size
								{
									_RxQueue.erase(_RxQueue.begin(), _RxQueue.begin() + 1);
#ifdef __FSSM_DEBUG__
									std::cout << "\nATcommandControlledDiagInterface::run():   warning: maximum size of received messages buffer is reached ! Dropping oldest message.\n";
#endif
								}
								_RxQueue.push_back( msg );
							}
						}
					} while (lend_pos != std::string::npos);
				}
#ifdef __FSSM_DEBUG__
				else
				{
					std::cout << "\nATcommandControlledDiagInterface::run():   error: serialCOM::Read() failed.\n";
				}
#endif
			}
		}
#ifdef __FSSM_DEBUG__
		else
		{
			std::cout << "\nATcommandControlledDiagInterface::run():   error: serialCOM::GetNrOfBytesAvailable() failed.\n";
		}
#endif
		exit = _exit;
		_mutex.unlock();
		if (!exit)
			waitms(10);
	} while (!exit);
	_mutex.lock();
	_exit = false;
	_mutex.unlock();
}


std::string ATcommandControlledDiagInterface::writeRead(std::string command, unsigned int timeout)
{
	std::vector<char> buffer;
	std::string reply;
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::writeRead():   command " << command << ": ";
#endif
	// Send command:
	TimeM time;
	buffer.assign(command.begin(), command.end());
	buffer.push_back('\r');
	if (!transmitMessage(buffer, true))
		return "";
	// Get reply:
	time.start();
	do
	{
		waitms(10);
		_mutex.lock();
		if (_RxQueue.size())
		{
			reply = _RxQueue.at(0);
			_RxQueue.erase(_RxQueue.begin(), _RxQueue.begin()+1);
		}
		_mutex.unlock();
	} while ((time.elapsed() < timeout) && !reply.size());	
	if (!reply.size())
	{
		_mutex.lock();
		_ready = true;
		_mutex.unlock();
#ifdef __FSSM_DEBUG__
		std::cout << "[no reply]\n";
#endif
		return "";
	}
#ifdef __FSSM_DEBUG__
	std::cout << reply << "\n";
#endif
	return reply;
}


bool ATcommandControlledDiagInterface::transmitMessage(std::vector<char> msg, bool flush_rec)
{
	TimeM time;
	unsigned int t_el = 0;
	unsigned int T_Tx_min = 0;

	if (!msg.size())
		return false;
	T_Tx_min = static_cast<unsigned int>(1000 * msg.size() * 10 / _baudrate);	// NOTE: transmission time from interface to ECU neglected (this is just an approximation)
	time.start();
	_mutex.lock();
#ifdef __FSSM_DEBUG__
	if (!_ready)
		std::cout << "ATcommandControlledDiagInterface::transmitMessage():   warning: sending data without beeing in \"ready\" state !";
#endif
	// Flush Rx-queue and receive buffers
	// NOTE: Don't move this after sending the message ! The interface might reply very fast (especially with echo enabled) and we could miss some characters !
	if (flush_rec)
	{
		_RxQueue.clear();
		_flush_local_Rx_buffer = true;
		_port->ClearReceiveBuffer();
	}
	// Send data:
	if (!_port->Write(msg))
	{
		_ready = false;
		_mutex.unlock();
		return false;
	}
	_lastcmd.assign(msg.begin(), msg.end()-1);
	_mutex.unlock();
	// Wait the minimum time required to transmit the data
	if (T_Tx_min)
	{
		t_el = time.elapsed();
		if (t_el < T_Tx_min)
			waitms(T_Tx_min - t_el);
	}
	return true;
}


ATcommandControlledDiagInterface::if_model_dt ATcommandControlledDiagInterface::probeInterface()
{
	std::string reply;
	if_model_dt if_model = if_none;
	std::string if_name;
	std::string if_version;
	reply = writeRead("ATE0", 50);	// NOTE: interface replies even if baudrate is wrong (detects unknown command and replies "?" after a timeout)
	/* NOTE We just want to send a CR to terminate an incompletely transmitted command (e.g. due to a wrong baud rate). The reply may be garbage.
	 * But CR also means "repeat last command" and if there is no incompletely transmitted command and we are at the right baud rate,
	 * this could trigger a time intensive operation.
	 * So we use the fast ATE command and attemp to disable the interface echo so that we can disable the unsafe echo detection.
	 */
	if (!reply.size()) 	// no reply
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::probeInterface():   no response from device.\n";
#endif
		return if_none;
	}

	/* NOTE: Interfaces might have a custom identification string activated. Some interfaces provide a command (e.g. the AGV4000B: ATOMY0)
	 * to switch back to the original ID. But at this point, we don't know yet which device we're talking to and we don't want to issue a command
	 * which might have a different meaning for other devices. If the user changes the ID of the interface, it's his fault. 			*/
	// Request identification string
	reply = writeRead("ATI");
	/* Interfaces replies: e.g. "ELM329 v1.0", "ELM323 v2.0", "ELM327 v1.3a", "ELM327 v1.4b", ...
	 * or "ELM32x compatible AGV2055 v5.0" "ELMsfire AGV2055 v4.0", "AGV4000 v5.0", "DIAMEX DX61 v1.0", "DIAMEX DX35 v1.0", ...
	 */
	if ((reply.find("AGV") != std::string::npos) || (reply.find("DIAMEX") != std::string::npos))
	{
		if_model = if_model_AGV;
		reply = writeRead("AT!01"); // reply: e.g. "AGV2055-40", "AGV4000-50", "DX61-10", "DX35-10", AGV4000B-108, ...
		if (reply != "?")
		{
			if ((reply.find("AGV2055") != std::string::npos) || (reply.find("AGV3000") != std::string::npos) || (reply.find("AGV3100") != std::string::npos) || (reply.find("AGV3500") != std::string::npos))
			{
#ifdef __FSSM_DEBUG__
				std::cout << "ATcommandControlledDiagInterface::probeInterface():   error: unsuitable AGV-interface detected.\n";
#endif
				return if_unsupported;
			}
			else if (reply.find("AGV4000B") != std::string::npos)
			{
				if_model = if_model_AGV4000B;
			}
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::probeInterface():   AGV/Diamex-interface detetced.\n";
#endif
			// Get interface name and version
			size_t pos = reply.rfind("-");
			if ((pos == (reply.size() - 3)) || (pos == (reply.size() - 4)))	// NOTE: some AGV4000B with older firmware (< v1.1) use 3 version digits instead of 2
			{
				if_name = reply.substr(0, pos);
				if_version = reply.substr(pos+1, 1);
				if_version += '.';
				if_version += reply.substr(pos+2, 1);
				if (pos == (reply.size() - 4))
				{
					if_version += '.';
					if_version += reply.substr(pos+3, 1);
				}
			}
#ifdef __FSSM_DEBUG__
			else
				std::cout << "ATcommandControlledDiagInterface::probeInterface():   error: failed to extract AGV/Diamex-interface name and version string.\n";
#endif
		}
#ifdef __FSSM_DEBUG__
		else
			std::cout << "ATcommandControlledDiagInterface::probeInterface():   warning: interface seems to be of type AGV/Diamex, but command AT!01 is not supported !\n";
#endif
	}
	else if (reply.find("ELM") != std::string::npos)
	{
		if ((reply.find("ELM320") != std::string::npos) || (reply.find("ELM322") != std::string::npos) || (reply.find("ELM323") != std::string::npos))
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::probeInterface():   error: unsuitable ELM32x-interface detected.\n";
#endif
			return if_unsupported;
		}
		// Identify supported interfaces:
		if (reply.find("ELM327") != std::string::npos)
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::probeInterface():   ELM327-interface detetced.\n";
#endif
			if_model = if_model_ELM327;
		}
		else if (reply.find("ELM329") != std::string::npos)
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::probeInterface():   ELM329-interface detetced.\n";
#endif
			if_model = if_model_ELM329;
		}
		else
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::probeInterface():   unknown ELM-interface detetced. WARNING: this interface might not work ! Please contact the authors.\n";
#endif
			if_model = if_model_ELM327;
		}
		// Get interface name and version
		size_t pos_v = reply.rfind(" v");
		size_t pos_dot = reply.rfind(".");
		if ((pos_v != std::string::npos) && (pos_dot != std::string::npos) && ((pos_dot - pos_v) > 2) && ((reply.size() - pos_v) >= 5))
		{
			if_name = reply.substr(0, pos_v);
			if_version = reply.substr(pos_v+2, reply.size()-(pos_v+2));
		}
#ifdef __FSSM_DEBUG__
		else
			std::cout << "ATcommandControlledDiagInterface::probeInterface():   error: failed to extract ELM-interface name and version string.\n";
#endif
	}
	else	// Device unknown / unsupported
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::probeInterface():   error: unknown device detected. If this is an compatible interface an you would like to add support for it, please contact the authors. \n";
#endif
		return if_unsupported;
	}
	// Save interface name and version
	setName( if_name );
	setVersion( if_version );
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::probeInterface():   interface name:    " << if_name << '\n';
	std::cout << "                                                      interface version: " << if_version << '\n';
#endif
	return if_model;
}


bool ATcommandControlledDiagInterface::configureDevice()
{
	std::string reply;
	// Reset interface (warm start)
	reply = writeRead("ATWS");	// NOTE: slow !
	if (!reply.size())
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::configureDevice():   error: failed to reset the interface !\n";
#endif
		return false;
	}
	// Try to disable interface echo
	reply = writeRead("ATE0");
	if (reply == "OK")
	{
		_mutex.lock();
		_try_echo_detection = false;
		_mutex.unlock();
	}
#ifdef __FSSM_DEBUG__
	else
		std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed to disable interface echo !\n";	// see comment in run()
#endif
	// Disable linefeed
	if (writeRead("ATL0") == "OK") // NOTE: processRecData() and changeInterfaceBaudRate() can handle both cases (on/off), but need to know the current setting
		_linefeed_enabled = false;
	else
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::configureDevice():   error: failed to determine linefeed setting !\n";
#endif
		return false;
	}
	// Forget critical events (let critical events not prevent the interface from working in some cases)
	if ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM329))
	{
		reply = writeRead("ATFE");	// NOTE: ELM327 since v1.3a
#ifdef __FSSM_DEBUG__
		if (reply != "OK")
			std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed enable forgetting of critical events: "<< reply << "\n";
#endif
	}
	// Configure error output
	if (_if_model == if_model_AGV4000B)
	{
		// Enable printing error numbers/descriptions
		reply = writeRead("ATOSY1");
#ifdef __FSSM_DEBUG__
		if (reply != "OK")
			std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed to enable detasiled error descriptions !\n";
#endif
		// Enable printing of detailed error descrioptions (otherwise: error numbers)
		reply = writeRead("ATOEN0");
#ifdef __FSSM_DEBUG__
		if (reply != "OK")
			std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed to enable detasiled error descriptions !\n";
#endif
	}
	// Configure interface output format
	if ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM329))
	{
		// Use space character for byte separation (for now)
		reply = writeRead("ATS1");
#ifdef __FSSM_DEBUG__
		if (reply != "OK")
			std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed to set space character setting !\n";
#endif
	}
	else if (_if_model == if_model_AGV4000B)
	{
		// Set microcontroller output format // NOTE: = ATH0 + ATOHS1	(with ATOF1, at least ATH settings are ignored !)
		reply = writeRead("ATOF0");
#ifdef __FSSM_DEBUG__
		if (reply != "OK")
			std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed to set microcontroller output format !\n";
#endif
		// Use space character for byte separation (for now)
		reply = writeRead("ATOHS1");
#ifdef __FSSM_DEBUG__
		if (reply != "OK")
			std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed to set space character setting !\n";
#endif
		// Force upper case characters
		reply = writeRead("ATOUC1");
#ifdef __FSSM_DEBUG__
		if (reply != "OK")
			std::cout << "ATcommandControlledDiagInterface::configureDevice():   warning: failed to enable forced upper case characters !\n";
#endif
	}
	// Increase baud rate if interface supports custom baud rates
	if ((_baudrate = 38400) && ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM327)))
	{
		if ((_if_model == if_model_ELM327) || (_if_model == if_model_ELM327))
		{
			if (changeInterfaceBaudRate(CUSTOM_BAUDRATE))
				_custom_baudrate = true;
		}
	}
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::configureDevice():   completed.\n";
#endif
	return true;
}


std::vector<char> ATcommandControlledDiagInterface::processRecData(std::string datamsg)
{
	// NOTE: only for data replies !

	std::vector<char> processedData;
	std::vector< std::string > lines;
	// STEP 1: section to lines:
	unsigned int pos_a = 0;
	unsigned int pos_b = 0;
	for (unsigned int c=0; c<datamsg.size(); c++)
	{
		bool endl = false;
		if (!_linefeed_enabled && (datamsg.at(c) == '\r'))
		{
			endl = true;
			pos_b = c - _linefeed_enabled;
		}
		else if (_linefeed_enabled && (datamsg.at(c) == '\n') && (c > 0))
		{
			if (datamsg.at(c-1) == '\r')
			{
				endl = true;
				pos_b = c - _linefeed_enabled;
			}
		}
		else if (c == (datamsg.size() - 1))
		{
			endl = true;
			pos_b = c + 1;
		}
		if (endl)
		{
			std::string line;
			line.assign(datamsg.begin()+pos_a, datamsg.begin()+pos_b);
			if (line.size() > 0)
				lines.push_back(line);
			pos_a = c + 1;
		}
	}
	// NOTE: last line is not terminated
	if (!lines.size())
		return std::vector<char>();

	// STEP 2: Check if there was a bus initialization attempt and delete status info lines
	/* NOTE: possible messages ([NL] = \r or \r\n depending on the interface settings made with command ATL)
	   BUS INIT: ...[NL]UNABLE TO CONNECT			normal connect (sending data)
	   BUS INIT: ...OK[NL][DATA]				normal connect (sending data)
	   BUS INIT: ...ERROR					slow init
	   BUS INIT: ...OK					slow init (triggert with command ATSI)
	   BUS INIT: ERROR					fast init
	   BUS INIT: OK						fast init (triggert with command ATFI)
	   SEARCHING...[NL]UNABLE TO CONNECT		
	   SEARCHING...[NL][DATA]
	*/
	if ((lines.at(0).find("BUS INIT: ") == 0) || (lines.at(0).find("SEARCHING") == 0))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::processRecData:   bus initialization message detected, result = ";
#endif
		if ((lines.at(0).find("UNABLE TO CONNECT") != std::string::npos) || (lines.at(0).find("ERROR") != std::string::npos))
		{
#ifdef __FSSM_DEBUG__
			std::cout << "error.\n";
#endif
			return std::vector<char>();
		}
		else if (lines.at(0).find("OK") != std::string::npos)
		{
#ifdef __FSSM_DEBUG__
			std::cout << "success.\n";
#endif
		}
		else if (lines.size() > 1)	// Check second line for result string
		{
			if ((lines.at(1).find("UNABLE TO CONNECT") != std::string::npos) || (lines.at(1).find("ERROR") != std::string::npos))
			{
#ifdef __FSSM_DEBUG__
				std::cout << "error.\n";
#endif
				return std::vector<char>();
			}
			else if (lines.at(1).find("OK") != std::string::npos)
			{
#ifdef __FSSM_DEBUG__
				std::cout << "success.\n";
#endif
				lines.erase(lines.begin() + 1, lines.begin() + 2);
			}
#ifdef __FSSM_DEBUG__
			else
				std::cout << " UNKNOWN. Please report this as a bug !\n";
#endif
		}
		lines.erase(lines.begin(), lines.begin() + 1);
	}

	// NOTE: data format depends on the interface header settings !
	unsigned int num_databytes_total = 0;
	if (lines.size() == 1)
	{
		// STEP 3: convert to data
		std::vector<char> data;
		data = hexStrToData(lines.at(0));
		if (!data.size())
			return std::vector<char>();
		// STEP 4: check line
		if (_headers_enabled)
		{
			if (data.at(2) > 0x07)
				return std::vector<char>();	// Error: invalid first frame indicator
		}
		// Step 5: extract data and return data vector
		if (!_headers_enabled)
		{
			num_databytes_total = data.size();
			processedData.insert(processedData.end(), data.begin(), data.end());
		}
		else
		{
			num_databytes_total = data.at(2);
			processedData.insert(processedData.end(), data.begin()+3, data.end());
		}
	}
	else if (lines.size() > 1) // multiple lines (ISO15765 only)
	{
		if (!_headers_enabled)	// eliminate ':' character
		{
			for (unsigned int l=0; l<lines.size(); l++)
			{
				unsigned int line_len = lines.at(l).size();
				for (unsigned int c=0; c<lines.at(l).size(); c++)
				{
					if (lines.at(l).at(c) == ':')
					{
						if (l == 0)
							return std::vector<char>();	// NOTE: first line only contains 2 bytes representing the number of following data bytes
						if (c != 1)
							return std::vector<char>();	// NOTE: should always be the at position 2
						lines.at(l).erase( lines.at(l).begin() + c );
						c--;	// NOTE: dangerous ! works only as long as c is >= 1 !
					}
				}
				if ((line_len - lines.at(l).size()) != (l>0))
					return std::vector<char>();	// Error: more than one ':' detetced
			}
		}
		// STEP 3: convert to data
		std::vector< std::vector<char> > datalines;
		std::vector<char> dataline;
		for (unsigned int l=0; l<lines.size(); l++)
		{
			dataline = hexStrToData(lines.at(l));
			if (!dataline.size())
				return std::vector<char>();
			else
				datalines.push_back(dataline);
		}
		// STEP 4: check and sort lines
		/* NOTE: some datasheets (e.g. for the ELM329) say that the lines have to be ordered,
		 *       but ISO-15765 says that messages have to arrive in the right order (at least the consecutive frames)
		 *       ALSO: there are only 4 bits reserved for numbering of the consecutive frames,
		 *	       but more than 15 consecutive frames are possible !				*/
		// NOTE: DO INSTEAD: check first bytes 
		if (!_headers_enabled)
		{
			if (datalines.at(0).size() != 2)
				return std::vector<char>();
			for (unsigned int l=1; l<datalines.size(); l++)
			{
				if (static_cast<unsigned char>(datalines.at(l).at(0)) > 0x0f)
					return std::vector<char>();	// Error: consecutive frame number must be between 0 and 15
				if ((static_cast<unsigned char>(datalines.at(l).at(0)) & 0x0f) != (l-1))
					return std::vector<char>();	// Error: invalid consecutive frame number
				// NOTE: consecutive frame numbering starts with 0
			}
		}
		else
		{
			if (datalines.at(0).size() < 10)	// NOTE: always 10 characters per line
				return std::vector<char>();		// Error: invalid line size
			if ((static_cast<unsigned char>(datalines.at(0).at(2)) & 0xf0) != 0x10)
				return std::vector<char>();		// Error: first frame indicator must be 1y
			for (unsigned int l=1; l<datalines.size(); l++)
			{
				if ((static_cast<unsigned char>(datalines.at(l).at(2)) & 0xf0) != 0x20)
					return std::vector<char>();	// Error: consecutive frame indicator number must be 2y
				if ((static_cast<unsigned char>(datalines.at(l).at(2)) & 0x0f) != l)
					return std::vector<char>();	// Error: invalid consecutive frame number
				// NOTE: consecutive frame numbering starts with 1
			}
		}
		// Step 5: extract data and return data vector
		if (_headers_enabled)
		{
			num_databytes_total = (static_cast<unsigned char>(datalines.at(0).at(2)) & 0xf)*256 + static_cast<unsigned char>(datalines.at(0).at(3));
			for (unsigned int l=0; l<lines.size(); l++)
				processedData.insert(processedData.end(), datalines.at(l).begin() + 3 + (l==0), datalines.at(l).end());
		}
		else
		{
			num_databytes_total = static_cast<unsigned char>(datalines.at(0).at(0))*256 + static_cast<unsigned char>(datalines.at(0).at(1));
			for (unsigned int l=1; l<datalines.size(); l++)
				processedData.insert(processedData.end(), datalines.at(l).begin() + 1, datalines.at(l).end());
		}
	}
	else
		return std::vector<char>();

	// Check number of data bytes, remove padding bytes
	int paddingbytes = processedData.size() - num_databytes_total;
	if (paddingbytes > 0)
	{
		if ((lines.size() == 1) && (paddingbytes))	// NOTE: happens when headers are enabled and invalid data length bits
			return std::vector<char>();	// Error: invalid data length bits
		processedData.erase(processedData.end() - paddingbytes, processedData.end());
	}
	else if (paddingbytes < 0)
	{
		return std::vector<char>();	// Error: number of extracted data bytes is wrong
	}

	return processedData;
	
	/* NOTE: there's a big potential for optimizations, but we want to keep the code readable (and understandable) */
}


bool ATcommandControlledDiagInterface::changeCUAddresses(unsigned int target_addr)
{
	std::string cmd;
	std::string reply;
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::changeCUAddresses():   changing control unit target address to 0x" << std::hex << target_addr << "\n";
#endif
	// Check address range (currently supported: OBD2 address range for 11bit CAN-ID)
	if ((target_addr < 0x7E0) || (target_addr > 0x7E7))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeCUAddresses():   error: currently only control unit addresses from 0x7E0 to 0x7E7 are supported.\n";
#endif
		return false;
	}
	// Configure header/CAN-ID
	if (_if_model == if_model_AGV)
	{
		cmd = "ATCI";	// AGV4000+AGV4500+DX35+DX60/61
	}
	else if (_if_model == if_model_AGV4000B)
	{
		cmd = "ATCT";	// AGV4000B
	}
	else
	{
		cmd = "ATSH";	// ELM327+ELM329
	}
	cmd += "7E";
	cmd += ((target_addr & 0xf) + 48);	// NOTE: do not merge these 2 lines
	reply = writeRead(cmd);
	if (reply != "OK")
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeCUAddresses():   error: failed to set CAN-ID for outgoing messages\n";
#endif
		return false;
	}
	// Set CAN-ID filter and mask
	if (_if_model == if_model_AGV4000B)
	{
		cmd = "ATCR";
		cmd += "7E";
		cmd += dataToHexStr( std::vector<char>( 1, static_cast<char>((target_addr+8) & 0xff) ) ).at(1);	// NOTE: do not merge these 2 lines
		cmd += '-';
		cmd += "7E";
		cmd += dataToHexStr( std::vector<char>( 1, static_cast<char>((target_addr+8) & 0xff) ) ).at(1);	// NOTE: do not merge these 2 lines
		reply = writeRead(cmd);
		if (reply != "OK")
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::changeCUAddresses():   error: failed to set CAN-ID filter address for incoming messages !\n";
#endif
			return false;
		}
	}
	else
	{
		reply = writeRead("ATCM7EF");	// Set CAN-ID filter mask
		if (reply != "OK")
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::changeCUAddresses():   error: failed to set CAN-ID filter mask for incoming messages !\n";
#endif
			return false;
		}
		cmd = "ATCF";
		cmd += "7E";
		cmd += dataToHexStr( std::vector<char>( 1, static_cast<char>((target_addr+8) & 0xff) ) ).at(1);	// NOTE: do not merge these 2 lines
		reply = writeRead(cmd);		// Set CAN-ID filter address
		if (reply != "OK")
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::changeCUAddresses():   error: failed to set CAN-ID filter address for incoming messages !\n";
#endif
			return false;
		}
	}
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::changeCUAddresses():   successfully changed the device addresses.\n";
#endif
	_target_addr = target_addr;
	return true;
	// TODO: EXTEND ADDRESS RANGE
}


bool ATcommandControlledDiagInterface::changeInterfaceBaudRate(unsigned int baudrate)
{
	std::string cmd;
	std::string reply;
	unsigned int divisor = 0;
	std::string divisorstr;
	double act_br = 0;
	std::string idstr;
	std::vector<char> buf;
	unsigned int ttreset = 0;
	unsigned char readlen = 0;

#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   trying to set interface baud rate to " << std::dec << baudrate << "...\n";
#endif
	if (baudrate < 4000000.0/255) // A baud rate divisor > 255 doesn't make sense and probably not supported
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: baud rate is too small !\n";
#endif
		return false;
	}
	// Calculate baud rate divisor
	divisor = round(4000000.0 / baudrate);
	// Convert divisor to hex string
	divisorstr = dataToHexStr(std::vector<char>(1, divisor));
	if (divisorstr.size() != 2)
		return false;
	// Save current ID string
	idstr = writeRead("ATI");
	// Change timout value for baud rate switching procedure:
	cmd = "ATBRT";
	cmd += dataToHexStr(std::vector<char>(1, BRC_HS_TIMEOUT/5));
	reply = writeRead(cmd);
#ifdef __FSSM_DEBUG__
	if (reply != "OK")
	{
		if (reply == "?")	// ELM327: supported since v1.2
		{
			std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   interface does not support custom baud rates.\n";
			return false;
		}
		else	
			std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   warning: command ATBRT failed !\n";
	}
#endif
	// Send baud rate change request to interface 
	/* NOTE: PROBLEM: interface does not terminate messages with an empty line during baud rate switching process,
	 * so message extraction in run() doesn't work and writeRead() fails
	 * We could modify run() to temporarily extract single lines as messages, but that seems to be overkill. */
	cmd = "ATBRD";
	cmd += divisorstr + '\r';
	buf.assign(cmd.begin(), cmd.end());
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   sending baud rate change request " << cmd << '\n';
#endif
	_mutex.lock();
	if (!_port->Write(buf))
	{
		_mutex.unlock();
		return false;
	}
	// Wait for confirmation from interface
	readlen = 1 + _try_echo_detection*( (cmd.size()-1) + (1+_linefeed_enabled) ) + 2;	// minimum nr. of characters to read
	if (!_port->Read(readlen, readlen, BRC_HS_TIMEOUT, &buf))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: failed to read from serial port !\n";
#endif
		ttreset = 2*BRC_HS_TIMEOUT;
		goto err;
	}
	if (buf.size() != readlen)
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: the interface did not confirm the baud rate change (timeout) !\n";
#endif
		ttreset = 2*BRC_HS_TIMEOUT;
		goto err;
	}
	// Eliminate "ready" character (remaining from reply)
	if (buf.at(0) == '>')
		reply.assign(buf.begin()+1, buf.end());
	else
		reply.assign(buf.begin(), buf.end());
	// Eliminate echo
	if (_try_echo_detection)
		reply.erase(reply.begin(), reply.begin() + (cmd.size()-1) + (1+_linefeed_enabled));
	// Check reply
	if (reply.at(0) == '?')  // NOTE: the ELM327 before v1.2 does not support this command / custom baud rates
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   interface does not support custom baud rates (command ATBRD not supported).\n";
#endif
		goto err;
	}
	else if (reply.find("OK") != 0)
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: the interface did not confirm the baud rate change request !\n";
#endif
		ttreset = 2*BRC_HS_TIMEOUT; // to be sure
		goto err;
	}
	else // "OK", everything is fine
	{
		readlen = (2 + 1 + _linefeed_enabled) - reply.size(); // calculate remaining characters, before the ID-string is expected
		// NOTE: datasheet says that the interface sends a single CR, but it also sends a LF if linefeed is enabled
		// Read rest of the message:
		if (!_port->Read(readlen, readlen, 10, &buf))
		{
#ifdef __FSSM_DEBUG__
			std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: failed to read from serial port !\n";
#endif
			ttreset = BRC_HS_TIMEOUT;
			goto err_reset_port;
		}
	}
	// Switch port baud rate
	act_br = 4000000.0 / divisor;
	if (!_port->SetPortSettings(act_br, 8, 'N', 1))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: failed to switch serial port baud rate !\n";
#endif
		ttreset = BRC_HS_TIMEOUT;
		goto err;
	}
	// Wait until interface sends ID-string:
	if (!_port->Read(idstr.size() + 1 + _linefeed_enabled, 255, 150+30, &buf))	// NOTE: first character should be received after 150ms
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: failed to read from serial port !\n";
#endif
		ttreset = BRC_HS_TIMEOUT;
		goto err_reset_port;
	}
	reply.assign(buf.begin(), buf.end());
	// Reset receive buffers and return to normal meassage reading mode
	_port->ClearReceiveBuffer();
	_RxQueue.clear();
	_flush_local_Rx_buffer = true;	// let read-thread flush local buffers before next read
	_mutex.unlock();
	// Validate ID-string
	if (reply.find(idstr) == 0)
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   valid ID-string received at the new baud rate, sending confirmation.\n";
#endif
		// Send CR to confirm new baud rate
		reply = writeRead("");
	}
	else
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   invalid ID-string received at the new baud rate: " << reply << '\n';
#endif
		reply.clear();
	}
	// Switch back to initial baud rate if necessary
	if (reply != "OK")
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   error: failed to switch interface baud rate to " << std::dec << baudrate << " baud !\n";
#endif
		_mutex.lock();
		goto err_reset_port;
	}
	_baudrate = act_br;
#ifdef __FSSM_DEBUG__
	std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   successfully switched to " << std::dec << baudrate << " baud.\n";
#endif
	return true;

err_reset_port:
	// Reset serial port to initial baud rate
	if (!_port->SetPortSettings(38400, 8, 'N', 1))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "ATcommandControlledDiagInterface::changeInterfaceBaudRate():   critical error: failed to reset baud rate to 38400. The interface may be in an unusable state !\n";
#endif
	}
err:
	// Wait until interface times out and switches back to the initial baud rate (if necessary)
	if (ttreset)
		waitms(ttreset + 30);
	// Reset receive buffers and return to normal meassage reading mode
	_port->ClearReceiveBuffer();
	_RxQueue.clear();
	_flush_local_Rx_buffer = true;	// let read-thread flush local buffers before next read
	_mutex.unlock();
	return false;
}


std::string ATcommandControlledDiagInterface::dataToHexStr(std::vector<char> data)
{
	std::string hexstr;
	unsigned short int charval = 0;
	unsigned char hexsigns[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	unsigned int bc = 1;
	for (bc=0; bc<data.size(); bc++)
	{
		charval = static_cast<unsigned char>(data.at(bc));
		hexstr += hexsigns[charval/16];
		hexstr += hexsigns[charval % 16];
		// separate bytes with space	// optional, not necessarily needed
		if (bc != data.size() - 1)
			hexstr += ' ';
	}
	return hexstr;
}


std::vector<char> ATcommandControlledDiagInterface::hexStrToData(std::string hexstr)
{
	std::vector<char> data;
	unsigned int pos_a = 0;
	unsigned int pos_b = 0;
	std::vector< std::vector<char> > blocks;
	unsigned char byteval = 0;
	unsigned char charval = 0;
	// Separate blocks
	for (unsigned int k=0; k<hexstr.size(); k++)
	{
		if ((hexstr.at(k) == ' ') || (k == hexstr.size()-1))
		{
			pos_b = k;
			if (hexstr.at(k) != ' ') // end of the string, but no trailing space
				pos_b++;
			std::vector<char> block;
			block.assign(hexstr.begin()+pos_a, hexstr.begin()+pos_b);
			pos_a = k + 1;
			if (block.size() > 0)
			{
				if ((block.size() % 2) != 0)
					block.insert( block.begin(), '0' );
				blocks.push_back(block);
			}
		}
	}
	// Convert to values (data)
	for (unsigned int b=0; b<blocks.size(); b++)
	{
		for (unsigned int c=0; c<blocks.at(b).size(); c++)
		{
			if ((blocks.at(b).at(c) >= 48) && (blocks.at(b).at(c) <= 57))	// charater 0-9
			{
				charval = static_cast<unsigned char>(blocks.at(b).at(c)) - 48;
			}
			else if ((blocks.at(b).at(c) >= 65) && (blocks.at(b).at(c) <= 70))	// charater a-f
			{
				charval = static_cast<unsigned char>(blocks.at(b).at(c)) - 55;
			}
			else if ((blocks.at(b).at(c) >= 97) && (blocks.at(b).at(c) <= 102))	// charater A-F
			{
				charval = static_cast<unsigned char>(blocks.at(b).at(c)) - 87;
			}
			else
			{
				return std::vector<char>();	// Error: string is no hex string (invalid character)
			}
			// Calculate byte value:
			byteval += ((c+1)%2)*charval*16 + !((c+1)%2)*charval;
			// Check if byte is complete
			if (!((c+1) % 2))
			{
				data.push_back(byteval);
				byteval = 0;
			}
		}		
	}
	return data;
}
