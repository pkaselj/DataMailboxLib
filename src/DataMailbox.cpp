#include "DataMailbox.hpp"

#include "Kernel.hpp"

#include "Time.hpp"

#include <cstring>
#include <sstream>
#include <fstream>

const std::string getDataTypeName(MessageDataType dataType)
{
	std::array<std::string, 8> names = { 
		"NONE"
		"TimedOut",
		"EmptyQueue",
		"KeypadMessage_wPassword",
		"KeypadMessage_wCommand",
		"RFIDMessage",
		"StringMessage",
		"WatchdogMessage"
	};

	return names.at((int)dataType); // Potential indexOutOfBounds exception
}

DataMailboxMessage::DataMailboxMessage()
	: m_dataType(MessageDataType::NONE),
	m_serialized(nullptr),
	m_sizeOfSerializedData(0)
{

}

DataMailboxMessage::DataMailboxMessage(MessageDataType dataType)
	: m_dataType(dataType),
	m_serialized(nullptr),
	m_sizeOfSerializedData(0)
{

}

DataMailboxMessage::~DataMailboxMessage()
{
	deleteSerializedData();
}

void DataMailboxMessage::DumpSerialData(const std::string filepath)
{
	if (m_serialized == nullptr)
	{
		Kernel::Warning("Trying to dump message with no serialized data! ");
		Serialize();
	}

	std::ofstream dump(filepath + ".dump", std::ios::out | std::ios::binary);
	if (!dump)
	{
		Kernel::Warning("Cannot dump message: " + getInfo());
		return;
	}

	dump.write(m_serialized, m_sizeOfSerializedData);

	Kernel::Trace("Serial data dumped!");

	// deleteSerializedData();

	dump.close();
}

void DataMailboxMessage::decodeMessageDataType()
{

	if (m_serialized == nullptr)
	{
		Kernel::Fatal_Error("Cannot decode message datatype from null pointer!");
	}

	memcpy(&m_dataType, m_serialized, sizeof(MessageDataType));

	if ((int)m_dataType < 0 || (int)m_dataType >= (int)MessageDataType::COUNT)
	{
		Kernel::DumpRawData(m_serialized, m_sizeOfSerializedData, "invalid_message_datatype_pid_" + std::to_string( getpid() ) );
		Kernel::Fatal_Error("Message has invalid datatype: " + std::to_string((int)m_dataType));
	}
		
}

void DataMailboxMessage::checkSerializedData()
{
	if (m_serialized == nullptr)
		Kernel::Fatal_Error("Cannot deserialize data from nullptr!");
}

void DataMailboxMessage::deleteSerializedData()
{
	if (m_serialized != nullptr)
	{
		delete[] m_serialized;
		m_serialized = nullptr;
	}

	m_sizeOfSerializedData = 0;
		
}

void DataMailboxMessage::deleteAndReallocateSerializedData(size_t size)
{

	deleteSerializedData();
	m_sizeOfSerializedData = size;
	m_serialized = new char[m_sizeOfSerializedData];
}

ExtendedDataMailboxMessage::ExtendedDataMailboxMessage(MessageDataType dataType)
	:	DataMailboxMessage(dataType)
{

}

void ExtendedDataMailboxMessage::Unpack(BasicDataMailboxMessage& message)
{
	m_sizeOfSerializedData = message.getRawDataSize();
	m_serialized = message.getRawDataPointer();
	message.releaseRawDataOwnership();

	Deserialize();

	m_source = message.getSource();

	deleteSerializedData();
}



DataMailbox::DataMailbox(const std::string name, ILogger* pLogger, const mq_attr& mailboxAttributes)
	: m_mailbox(name, pLogger, mailboxAttributes)
{
	if (pLogger == nullptr)
		pLogger = NulLogger::getInstance();

	m_pLogger = pLogger;

	*m_pLogger << "DataMailbox opened: " + name;
}

DataMailbox::~DataMailbox()
{
	*m_pLogger << "DataMailbox closed: " + m_mailbox.getName();
}

/*
void DataMailbox::clearMessageBuffer()
{
	if (m_receivedMessage != nullptr)
	{
		delete m_receivedMessage;
		m_receivedMessage = nullptr;
	}
}*/

void DataMailbox::send(MailboxReference& destination, DataMailboxMessage* message)
{

	*m_pLogger << m_mailbox.getName() + " - sending message to - " + destination.getName();

	std::stringstream stringbuilder;

	stringbuilder << "\n"
		<< "==========================================" << "\n"
		<< "Message: | " << message->getInfo() << "\n"
		<< "==========================================";

	*m_pLogger << stringbuilder.str();

	message->Serialize();

	m_mailbox.send(destination, message->m_serialized, message->m_sizeOfSerializedData);

	message->deleteSerializedData();

	*m_pLogger << m_mailbox.getName() + " - message successfully sent to - " + destination.getName();

}

void DataMailbox::sendConnectionless(MailboxReference& destination, DataMailboxMessage* message)
{
	*m_pLogger << m_mailbox.getName() + " - sending message to - " + destination.getName() + " - CONNECTIONLESS";

	std::stringstream stringbuilder;

	stringbuilder << "\n"
		<< "==========================================" << "\n"
		<< "Message: | " << message->getInfo() << "\n"
		<< "==========================================";

	*m_pLogger << stringbuilder.str();

	message->Serialize();

	m_mailbox.sendConnectionless(destination, message->m_serialized, message->m_sizeOfSerializedData);

	message->deleteSerializedData();

	*m_pLogger << m_mailbox.getName() + " - message successfully sent to - " + destination.getName();
}

struct timespec DataMailbox::setRTO_s(time_t RTOs)
{
	timespec oldSettings = getTimeout_settings();
	m_mailbox.setRTO_s(RTOs);
	return oldSettings;
}

struct timespec DataMailbox::setRTO_ns(long RTOns)
{
	timespec oldSettings = getTimeout_settings();
	m_mailbox.setRTO_ns(RTOns);
	return oldSettings;
}

void DataMailbox::setTimeout_settings(struct timespec _timeout_settings)
{
	m_mailbox.setTimeout_settings(_timeout_settings);
}

struct timespec DataMailbox::getTimeout_settings()
{
	return m_mailbox.getTimeout_settings();
}

mq_attr DataMailbox::getMQAttributes()
{
	return m_mailbox.getMQAttributes();
}

void DataMailbox::setMQAttributes(const mq_attr& message_queue_attributes)
{
	m_mailbox.setMQAttributes(message_queue_attributes);
}

BasicDataMailboxMessage DataMailbox::receive(enuReceiveOptions options)
{

	*m_pLogger << m_mailbox.getName() + " - waiting for message!";

	SimpleMailboxMessage rawMessage = m_mailbox.receive(options);

	//Kernel::DumpRawData(rawMessage.m_pData, rawMessage.m_header.m_payloadSize, "dump_rec_trace_1_" + Time::getTime());

	BasicDataMailboxMessage receivedMessage;

	if (rawMessage.m_header.m_type == enuMessageType::TIMED_OUT) // TODO change enum name
	{
		// std::cout << "TIMEDOUT" << std::endl;
		char* pData = new char[1]{ (char)MessageDataType::TimedOut }; // Emulate received message with datatype code = TimedOut
		receivedMessage.setSerializedData(pData, 1);
	}
	else if (rawMessage.m_header.m_type == enuMessageType::EMPTY && (options & enuReceiveOptions::NONBLOCKING) != 0)
	{
		// std::cout << "NONBLOCKING_EMPTY" << std::endl;
		char* pData = new char[1]{ (char)MessageDataType::EmptyQueue }; // Emulate received message with datatype code = EmptyQueue
		receivedMessage.setSerializedData(pData, 1);
	}
	else
	{
		// std::cout << "~TIMEDOUT" << std::endl;
		receivedMessage.setSerializedData(rawMessage.m_pData, rawMessage.m_header.m_payloadSize);
		receivedMessage.setSource(MailboxReference(rawMessage.m_sourceName));
	}

	receivedMessage.decodeMessageDataType();
	

	*m_pLogger << m_mailbox.getName() + " - message successfully received";

	std::stringstream stringbuilder;

	stringbuilder << "\n"
		<< "==========================================" << "\n"
		<< "Message: | " << receivedMessage.getInfo() << "\n"
		<< "==========================================";

	*m_pLogger << stringbuilder.str();

	return receivedMessage;
}

BasicDataMailboxMessage::BasicDataMailboxMessage()
{
	setSource(MailboxReference{});
}

BasicDataMailboxMessage::BasicDataMailboxMessage(MessageDataType dataType, const MailboxReference& source)
	:	DataMailboxMessage(dataType)
{
	setSource(source);
}

void BasicDataMailboxMessage::Serialize()
{
	deleteAndReallocateSerializedData(sizeof(MessageDataType));

	memcpy(&m_serialized, &m_dataType, m_sizeOfSerializedData);
}

void BasicDataMailboxMessage::Deserialize()
{
	checkSerializedData();

	decodeMessageDataType();
}

char* BasicDataMailboxMessage::getRawDataPointer() const
{
	return m_serialized;
}

int BasicDataMailboxMessage::getRawDataSize() const
{
	return m_sizeOfSerializedData;
}

void BasicDataMailboxMessage::releaseRawDataOwnership()
{
	m_serialized = 0;
	m_sizeOfSerializedData = 0;
}

/*
void BasicDataMailboxMessage::setSource(const MailboxReference& source)
{
	m_source = source;
}
*/

std::string BasicDataMailboxMessage::getInfo()
{
	return "BasicDataMailboxMessage - MessageDataType: " + std::to_string((int)m_dataType) + " from: " + m_source.getName();
}

KeypadMessage_wPassword::KeypadMessage_wPassword()
	:	m_password(""), ExtendedDataMailboxMessage(MessageDataType::KeypadMessage_wPassword)
{

}

KeypadMessage_wPassword::KeypadMessage_wPassword(const std::string& password)
	:	m_password(password), ExtendedDataMailboxMessage(MessageDataType::KeypadMessage_wPassword)
{

}

void KeypadMessage_wPassword::Serialize()
{

	/*
	int passwordLength = m_password.length();
	int passwordLengthOffset = sizeof(MessageDataType);

	int passwordOffset = passwordLengthOffset + sizeof(passwordLengthOffset);

	int sizeOfSerializedData = passwordOffset + m_password.length();

	deleteAndReallocateSerializedData(sizeOfSerializedData);

	memset(m_serialized, 0, sizeOfSerializedData);

	memcpy(m_serialized, &m_dataType, sizeof(MessageDataType));
	memcpy(m_serialized + passwordLengthOffset, &passwordLength, sizeof(passwordLength));
	memcpy(m_serialized + passwordOffset, m_password.c_str(), passwordLength);

	// m_sizeOfSerializedData = sizeOfSerializedData; // deleteAndReallocateSerializedData() sets the m_sizeOfSerializedData
	*/

	size_t passwordLength = m_password.length();
	size_t passwordOffset = sizeof(MessageDataType);

	size_t sizeOfSerializedData = sizeof(MessageDataType) + passwordLength;

	deleteAndReallocateSerializedData(sizeOfSerializedData);

	memcpy(m_serialized, &m_dataType, sizeof(MessageDataType));
	memcpy(m_serialized + passwordOffset, m_password.c_str(), passwordLength);
}

void KeypadMessage_wPassword::Deserialize()
{
	checkSerializedData();

	/*
	memcpy(&m_dataType, m_serialized, sizeof(MessageDataType));

	int passwordLength = 0;
	int passwordLenghtOffset = sizeof(MessageDataType);
	memcpy(&passwordLength, m_serialized + passwordLenghtOffset, sizeof(passwordLength));

	int passwordOffset = passwordLenghtOffset + sizeof(passwordLength);

	m_password = std::string(m_serialized + passwordOffset, passwordLength);
	*/

	size_t passwordOffset = sizeof(MessageDataType);
	size_t passwordLength = m_sizeOfSerializedData - passwordOffset;

	memcpy(&m_dataType, m_serialized, sizeof(MessageDataType));
	m_password = std::string(m_serialized + passwordOffset, passwordLength);

}

std::string KeypadMessage_wPassword::getInfo()
{
	return "KeypadMessage_wPassword - Password: " + m_password;
}

KeypadMessage_wCommand::KeypadMessage_wCommand()
	:	m_command(KeypadCommand::NONE), m_parameters(""), ExtendedDataMailboxMessage(MessageDataType::KeypadMessage_wCommand)
{

}

KeypadMessage_wCommand::KeypadMessage_wCommand(KeypadCommand commandId, const std::string& parameters)
	:	m_command(commandId), m_parameters(parameters), ExtendedDataMailboxMessage(MessageDataType::KeypadMessage_wCommand)
{

}

void KeypadMessage_wCommand::Serialize()
{

	size_t sizeOfSerializedData = sizeof(MessageDataType) + sizeof(KeypadCommand) + m_parameters.length();

	int commandIdOffset = sizeof(MessageDataType);
	int parametersOffset = sizeof(KeypadCommand) + commandIdOffset;

	int parametersLength = m_parameters.length();

	deleteAndReallocateSerializedData(sizeOfSerializedData);

	memcpy(reinterpret_cast<void*>(m_serialized), reinterpret_cast<const void*>(&m_dataType), sizeof(MessageDataType));
	memcpy(reinterpret_cast<void*>(m_serialized + commandIdOffset), reinterpret_cast<const void*>(&m_command), sizeof(KeypadCommand));
	memcpy(reinterpret_cast<void*>(m_serialized + parametersOffset), reinterpret_cast<const void*>(m_parameters.c_str()), parametersLength);

}

void KeypadMessage_wCommand::Deserialize()
{
	checkSerializedData();

	int commandIdOffset = sizeof(MessageDataType);
	int parametersOffset = sizeof(KeypadCommand) + commandIdOffset;

	int parametersLength = m_sizeOfSerializedData - parametersOffset;

	memcpy(reinterpret_cast<void*>(&m_command), reinterpret_cast<const void*>(m_serialized + commandIdOffset), sizeof(KeypadCommand));
	
	m_parameters = std::string(m_serialized + parametersOffset, parametersLength);
}

std::string KeypadMessage_wCommand::getInfo()
{
	return "KeypadMessage_wCommand - CommandId: " + std::to_string((int)m_command);
}

RFIDMessage::RFIDMessage()
	:	m_uuid(""), ExtendedDataMailboxMessage(MessageDataType::RFIDMessage)
{

}

RFIDMessage::RFIDMessage(const std::string& uuid)
	: m_uuid(uuid), ExtendedDataMailboxMessage(MessageDataType::RFIDMessage)
{

}

void RFIDMessage::Serialize()
{
	size_t sizeOfSerializedData = m_uuid.length() + sizeof(MessageDataType);
	int uuidLength = m_uuid.length();

	int uuidOffset = sizeof(MessageDataType);

	deleteAndReallocateSerializedData(sizeOfSerializedData);

	memcpy(reinterpret_cast<void*>(m_serialized), reinterpret_cast<const void*>(&m_dataType), sizeof(MessageDataType));
	memcpy(reinterpret_cast<void*>(m_serialized + uuidOffset), reinterpret_cast<const void*>(m_uuid.c_str()), uuidLength);
}

void RFIDMessage::Deserialize()
{
	checkSerializedData();

	int uuidOffset = sizeof(MessageDataType);

	int uuidLength = m_sizeOfSerializedData - uuidOffset;

	m_uuid = std::string(m_serialized + uuidOffset, uuidLength);
}

std::string RFIDMessage::getInfo()
{
	return "RFIDMessage - UUID: " + m_uuid;
}

StringMessage::StringMessage()
	: m_message(""), ExtendedDataMailboxMessage(MessageDataType::StringMessage)
{

}

StringMessage::StringMessage(const std::string& message)
	: m_message(message), ExtendedDataMailboxMessage(MessageDataType::StringMessage)
{

}

void StringMessage::Serialize()
{
	size_t sizeOfSerializedData = sizeof(MessageDataType) + m_message.length();
	int messageLength = m_message.length();

	int messageOffset = sizeof(MessageDataType);

	deleteAndReallocateSerializedData(sizeOfSerializedData);

	memcpy(reinterpret_cast<void*>(m_serialized), reinterpret_cast<const void*>(&m_dataType), sizeof(MessageDataType));
	memcpy(reinterpret_cast<void*>(m_serialized + messageOffset), reinterpret_cast<const void*>(m_message.c_str()), messageLength);
}

void StringMessage::Deserialize()
{
	checkSerializedData();

	m_message = std::string(m_serialized, m_sizeOfSerializedData);
}

std::string StringMessage::getInfo()
{
	return "StringMessage - message: " + m_message;
}



WatchdogMessage::WatchdogMessage()
	:	ExtendedDataMailboxMessage(MessageDataType::WatchdogMessage),
	m_name(""), m_messageClass(MessageClass::NONE), m_settings(), m_PID(0), m_onFailure(enuActionOnFailure::RESET_ONLY)
{

}

WatchdogMessage::WatchdogMessage(const std::string& name, const SlotSettings& settings, unsigned int PID, enuActionOnFailure onFailure, MessageClass type)
	: ExtendedDataMailboxMessage(MessageDataType::WatchdogMessage),
	m_name(name), m_settings(settings), m_messageClass(type), m_PID(PID), m_onFailure(onFailure)
{

}

WatchdogMessage::WatchdogMessage(const std::string& name,
	MessageClass type)
	: ExtendedDataMailboxMessage(MessageDataType::WatchdogMessage),
	m_name(name), m_messageClass(type), m_settings(), m_PID(0), m_onFailure(enuActionOnFailure::RESET_ONLY)
{

}

WatchdogMessage::WatchdogMessage(MessageClass type)
	:	ExtendedDataMailboxMessage(MessageDataType::WatchdogMessage),
	m_name(""), m_messageClass(MessageClass::NONE), m_settings(), m_PID(0), m_onFailure(enuActionOnFailure::RESET_ONLY)
{

}

void WatchdogMessage::Serialize()
{
	size_t messageClassOffset = sizeof(MessageDataType);
	size_t settingsOffset = messageClassOffset + sizeof(m_messageClass);
	size_t PID_Offset = settingsOffset + sizeof(m_settings);
	size_t actionOnFailureOffset = PID_Offset + sizeof(m_PID);
	size_t nameOffset = actionOnFailureOffset + sizeof(m_onFailure);
	
	size_t sizeOfSerializedData = nameOffset + m_name.length();

	deleteAndReallocateSerializedData(sizeOfSerializedData);

	memcpy(m_serialized, &m_dataType, sizeof(MessageDataType));
	memcpy(m_serialized + messageClassOffset, &m_messageClass, sizeof(m_messageClass));
	memcpy(m_serialized + settingsOffset, &m_settings, sizeof(m_settings));
	memcpy(m_serialized + PID_Offset, &m_PID, sizeof(m_PID));
	memcpy(m_serialized + actionOnFailureOffset, &m_onFailure, sizeof(m_onFailure));
	memcpy(m_serialized + nameOffset, m_name.c_str(), m_name.length());
}

void WatchdogMessage::Deserialize()
{
	checkSerializedData();

	size_t messageClassOffset = sizeof(MessageDataType);
	size_t settingsOffset = messageClassOffset + sizeof(m_messageClass);
	size_t PID_Offset = settingsOffset + sizeof(m_settings);
	size_t actionOnFailureOffset = PID_Offset + sizeof(m_PID);
	size_t nameOffset = actionOnFailureOffset + sizeof(m_onFailure);


	memcpy(&m_dataType, m_serialized, sizeof(MessageDataType));
	memcpy(&m_messageClass, m_serialized + messageClassOffset, sizeof(m_messageClass));
	memcpy(&m_settings, m_serialized + settingsOffset, sizeof(m_settings));
	memcpy(&m_PID, m_serialized + PID_Offset, sizeof(m_PID));
	memcpy(&m_onFailure, m_serialized + actionOnFailureOffset, sizeof(m_onFailure));

	m_name = std::string(m_serialized + nameOffset, m_sizeOfSerializedData - nameOffset);

}

std::string WatchdogMessage::getInfo()
{
	std::array<std::string, 2> onFailureActionNamesList = { "RESET_ONLY", "KILL_ALL" };

	std::stringstream stringBuilder;

	// std::cout << "m_onFailure" << (int)m_onFailure << std::endl;

	stringBuilder << "\n"
		<< "WatchdogSlotRequestMessage - from: " << m_name << "\n"
		<< "\tPID:" << m_PID << "\n"
		<< "\tType: " << getMessageClassName(m_messageClass) << "\n"
		<< "\tOn faliure: " << onFailureActionNamesList.at((int)m_onFailure) << "\n"
		<< "\tSettings:" << "\n"
		<< "\t\tBaseTTL: " << m_settings.m_BaseTTL << "\n"
		<< "\t\tTimeout: " << m_settings.m_timeout_ms << " ms" << "\n";

	return stringBuilder.str();
}

std::string WatchdogMessage::getMessageClassName(MessageClass messageClass)
{
	std::array<std::string, 14> m_messageClassNames =
	{
		"REGISTER_REQUEST",
		"REGISTER_REPLY",
		"UNREGISTER_REQUEST",
		"UNREGISTER_REPLY",
		"UPDATE_SETTINGS",
		"KICK",
		"START",
		"STOP",
		"SYNC_REQUEST",
		"SYNC_BROADCAST",
		"TERMINATE_REQUEST",
		"TERMINATE_BROADCAST",
		"ANY",
		"NONE"
	};

	return m_messageClassNames.at((int)messageClass);
}