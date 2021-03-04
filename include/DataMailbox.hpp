/*****************************************************************//**
 * \file   DataMailbox.hpp
 * \brief  .
 * 
 * \author KASO
 * \date   February 2021
 *********************************************************************/

#ifndef DATA_MAILBOX_HPP
#define DATA_MAILBOX_HPP

#include "SimplifiedMailbox.hpp"
#include "WatchdogSettings.hpp"

#include <string>


class DataMailbox;
class DataMailboxMessage;
class BasicDataMailboxMessage;
class ExtendedDataMailboxMessage;
class KeypadMessage_wPassword;
class KeypadMessage_wCommand;
class RFIDMessage;
class StringMessage;

/// Defines all the types of messages that can be sent and received with DataMailbox. Always the first byte of the raw (serialized) message.
enum class MessageDataType : char
{
	NONE = 0,
	EmptyQueue,
	TimedOut,
	KeypadMessage_wPassword,
	KeypadMessage_wCommand,
	RFIDMessage,
	StringMessage,
	WatchdogMessage,
	COUNT // get number of message types
};

/// Returns string name of the MessageDataType represented by `dataType` code. \see MessageDataType
const std::string getDataTypeName(MessageDataType dataType);

/**
 * @brief Apstract base class for all the message classes that can be sent and received with DataMailbox.
*/
class DataMailboxMessage
{
public:
	DataMailboxMessage();

	/// Used by constructors of inherited classes
	DataMailboxMessage(MessageDataType dataType);

	/// Deletes (frees) serialized data when called
	virtual ~DataMailboxMessage();

	/**
	 * @brief Function which serializes object data into compact binary form and stores it to m_serialized.
	 *
	 * How to overload: \n
	 * Before data can be serialized, `deleteAndReallocateSerializedData(size_t size)` must be called. \n
	 * \see `deleteAndReallocateSerializedData(size_t size)` \n
	 * \n
	 * First byte of the serialized data must be `m_dataType` which determines the type (class) of the message object. \n
	 * m_dataType is used to determine the proper course of action during deserialization and processing. \n
	 * \n
	 * Example serialization function for `class KeypadMessage_wCommand`: \n
	 *
	 *		class KeypadMessage_wCommand
	 *		{
	 *			//(...)
	 *				typedef enum : char
	 *				{
	 *					NONE = 0,
	 *					ADD_USER,
	 *					REMOVE_USER
	 *				} KeypadCommand;
	 *
	 *				virtual void Serialize();
	 *
	 *				KeypadCommand m_command;
	 *				std::string m_parameters;
	 *
	 *				// (...) and other inherited members like `m_serialized` and `deleteAndReallocateSerializedData(size_t size)`
	 *		};
	 *
	 *
	 *		void KeypadMessage_wCommand::Serialize()
	 *		{
	 *
	 *			size_t sizeOfSerializedData = sizeof(MessageDataType) + sizeof(KeypadCommand) + m_parameters.length();
	 *
	 *			int commandIdOffset = sizeof(MessageDataType);
	 *			int parametersOffset = sizeof(KeypadCommand) + commandIdOffset;
	 *
	 *			int parametersLength = m_parameters.length();
	 *
	 *			deleteAndReallocateSerializedData(sizeOfSerializedData);
	 *
	 *			memcpy(reinterpret_cast<void*>(m_serialized), reinterpret_cast<const void*>(&m_dataType), sizeof(MessageDataType));
	 *			memcpy(reinterpret_cast<void*>(m_serialized + commandIdOffset), reinterpret_cast<const void*>(&m_command), sizeof(KeypadCommand));
	 *			memcpy(reinterpret_cast<void*>(m_serialized + parametersOffset), reinterpret_cast<const void*>(m_parameters.c_str()), parametersLength);
	 *
	 *		}
	 *
	 *
	 *
	*/
	virtual void Serialize() = 0; // TODO

	/**
	 * @brief Function which deserializes object data from compact binary form from m_serialized and initialized object fields.
	 *
	 * How to overload: \n
	 * Before data can be deserialized, `checkSerializedData()` must be called. \n
	 * \see `checkSerializedData()` \n
	 * \n
	 * First byte of the deserialized data is the `m_dataType` which determines the type (class) of the message object. \n
	 * m_dataType is used to determine the proper course of action during deserialization and processing. \n
	 * \n
	 * m_sizeOfSerializedData can be used as it is automatically calculated at the reception by DataMailbox. \n
	 * \n
	 * Example deserialization function for `class KeypadMessage_wCommand`: \n
	 *
	 *		class KeypadMessage_wCommand
	 *		{
	 *			//(...)
	 *				typedef enum : char
	 *				{
	 *					NONE = 0,
	 *					ADD_USER,
	 *					REMOVE_USER
	 *				} KeypadCommand;
	 *
	 *				virtual void Deserialize();
	 *
	 *				KeypadCommand m_command;
	 *				std::string m_parameters;
	 *
	 *				// (...) and other inherited members like `m_serialized` and `deleteAndReallocateSerializedData(size_t size)`
	 *		};
	 *
	 *
	 *		void KeypadMessage_wCommand::Deserialize()
	 *		{
	 *			checkSerializedData();
	 *
	 *			int commandIdOffset = sizeof(MessageDataType);
	 *			int parametersOffset = sizeof(KeypadCommand) + commandIdOffset;
	 *
	 *			int parametersLength = m_sizeOfSerializedData - parametersOffset;
	 *
	 *			memcpy(reinterpret_cast<void*>(&m_command), reinterpret_cast<const void*>(m_serialized + commandIdOffset), sizeof(KeypadCommand));
	 *
	 *			m_parameters = std::string(m_serialized + parametersOffset, parametersLength);
	 *		}
	 *
	 *
	 *
	*/

	virtual void Deserialize() = 0; // TODO

	/// Dumps raw serialized data into `filepath.dump` file which can be read by any hex editor. Serializes data if necessary
	void DumpSerialData(const std::string filepath);

	/// Returns the MessageDataType which identifies the message class/object. Used when deserializing and processing data.
	MessageDataType getDataType() { return m_dataType; };

	/// Decodes MessageDataType from the first byte of the serialized raw binary data
	void decodeMessageDataType();

	/// Function which returns string with log info of the current message object.
	virtual std::string getInfo() = 0;

	/// Set serialized data as `rawData` and size as `dataSize`. Used internally. TODO private?
	void setSerializedData(char* rawData, size_t dataSize) { m_serialized = rawData; m_sizeOfSerializedData = dataSize; }

	/// Returns MailboxReference of source of this message (object)
	MailboxReference& getSource() { return m_source; };

protected:
	MessageDataType m_dataType;

	char* m_serialized;
	size_t m_sizeOfSerializedData;

	MailboxReference m_source;

	/// Checks validity of serialized data. Exits on failure.
	void checkSerializedData();

	/// Clears and frees current serialized data and allocates memory for new serialized data. Also sets the `m_sizeOfSerializedData` to `size`
	void deleteAndReallocateSerializedData(size_t size);

	/// Clears and frees current serialized data
	void deleteSerializedData();

	friend class DataMailbox;
};

/// Abstract virtual class used as a parent class for all other user-defined message classes.
class ExtendedDataMailboxMessage : public DataMailboxMessage
{
public:
	ExtendedDataMailboxMessage() = default;
	ExtendedDataMailboxMessage(MessageDataType dataType);
	virtual ~ExtendedDataMailboxMessage() {};

	/**
	 * Takes the `BasicDataMailboxMessage message` which represents serialized message received by DataMailbox, \n
	 * creates new Message object (specific message decoded from `MessageDataType m_dataType` field of `message`) \n
	 * takes ownership of the serialized data and deserialized it.
	 *
	 * @param message Represents serialized message received by DataMailbox
	*/
	void Unpack(BasicDataMailboxMessage& message);

	virtual void Deserialize() = 0;
	virtual void Serialize() = 0;
	virtual std::string getInfo() = 0;

protected:

};

class DataMailbox
{
public:
	/**
	 * @brief Creates new DataMailbox object
	 * @param name Globally unique DataMailbox name.
	 * @param pLogger Pointer to a ILogger* inherited class to log information.
	 * @param mailboxAttributes DataMailbox attributes e.g. max message size and max message length. \see MailboxReference
	*/
	DataMailbox(const std::string name, ILogger* pLogger = NulLogger::getInstance(), const mq_attr& mailboxAttributes = MailboxReference::messageAttributes);
	~DataMailbox();

	// DataMailboxMessage* getReceivedMessagePointer() { return m_receivedMessage; }

	/**
	 * @brief Send `message` to `destination` DataMailbox
	 * @param destination MailboxReference to another DataMailbox
	 * @param message Pointer to a class derived from DataMailboxMessage which represents the message.
	*/ // TODO
	void send(MailboxReference& destination, DataMailboxMessage* message);

	void sendConnectionless(MailboxReference& destination, DataMailboxMessage* message);

	/**
	 * @brief Listens for messages until one is received.
	 * @return BasicDataMailboxMessage object which holds the serialized message and the message dataType. Unpacks to more specific message class.
	*/ // TODO
	BasicDataMailboxMessage receive(enuReceiveOptions timed = enuReceiveOptions::NORMAL);

	/**
	 * @brief Set the RTO of current mailbox (in seconds)
	 *
	 * @param RTO Request TimeOut
	 * @return struct timespec Previous value of the RTO (defined as timespec struct)
	 */
	struct timespec setRTO_s(time_t RTOs);

	/**
	 * @brief Set the RTO of current mailbox (in nanoseconds)
	 *
	 * @param RTOns Request TimeOut
	 * @return struct timespec Previous value of the RTO (defined as timespec struct)
	 */
	struct timespec setRTO_ns(long RTOns);

	/**
	 * @brief Set the Timeout settings object.
	 *
	 * @param _timeout_settings timespec struct with new timeout settings
	 */
	void setTimeout_settings(struct timespec _timeout_settings);

	/**
	 * @brief Set the Timeout settings object.
	 *
	 * @return struct timespec Value of the RTO (defined as timespec struct)
	 */
	struct timespec getTimeout_settings();

	/**
	 * @brief Get Message Queue (Mailbox) attributes
	 *
	 * @return mq_attr copy of the mq_attr object from current mailbox / message queue
	 */
	mq_attr getMQAttributes();

	/**
	 * @brief Set Message Queue (Mailbox) attributes
	 *
	 * @param message_queue_attributes reference to the mq_attr object of curretn mailbox / message queue
	 */
	void setMQAttributes(const mq_attr& message_queue_attributes);

private:
	ILogger* m_pLogger;

	SimplifiedMailbox m_mailbox;

	// DataMailboxMessage* m_receivedMessage;

	// void clearMessageBuffer();
};


class BasicDataMailboxMessage : public DataMailboxMessage
{
public:
	BasicDataMailboxMessage();
	BasicDataMailboxMessage(MessageDataType dataType, const MailboxReference& source);
	virtual ~BasicDataMailboxMessage() = default;

	virtual void Serialize();
	virtual void Deserialize();

	/// Returns pointer to `m_serialized` - serialized raw data
	char* getRawDataPointer() const;

	/// Returns size of serialized raw data in `m_serialized` in bytes
	int getRawDataSize() const;

	/**
	* Relesases ownership of `m_serialized`. \n
	* BE SURE TO RELEASE OWNERSHIP AFTER TAKING IT AS TO AVOID UNDEFINED BEHAVIOUR \n
	* WHEN IT GETS FREED IN THE DESTRUCTOR OF THE BasicDataMailboxMessage. \n
	* \n
	* TODO: SMART POINTERS
	*/
	void releaseRawDataOwnership();

	/// Sets the message source. Used internally.
	void setSource(const MailboxReference& source) { m_source = source; };
	virtual std::string getInfo();

private:
};


class KeypadMessage_wPassword : public ExtendedDataMailboxMessage
{
public:
	KeypadMessage_wPassword();
	KeypadMessage_wPassword(const std::string& password);
	virtual ~KeypadMessage_wPassword() {}

	std::string getPassword() const { return m_password; };

	virtual void Serialize();
	virtual void Deserialize();
	virtual std::string getInfo();


private:
	std::string m_password;


};

class KeypadMessage_wCommand : public ExtendedDataMailboxMessage
{
public:

	typedef enum : char
	{
		NONE = 0,
		ADD_USER,
		REMOVE_USER
	} KeypadCommand;

	KeypadMessage_wCommand(); // TODO Forbid sending with empty commandId
	KeypadMessage_wCommand(KeypadCommand commandId, const std::string& parameters = "");
	virtual ~KeypadMessage_wCommand() {}

	KeypadCommand getCommandId() const { return m_command; }
	std::string getParameters() const { return m_parameters; }

	virtual void Serialize();
	virtual void Deserialize();
	virtual std::string getInfo();

private:

	KeypadCommand m_command;
	std::string m_parameters;

};

class RFIDMessage : public ExtendedDataMailboxMessage
{
public:
	RFIDMessage();
	RFIDMessage(const std::string& uuid);
	virtual ~RFIDMessage() {}

	std::string getUUID() const { return m_uuid; }

	virtual void Serialize();
	virtual void Deserialize();
	virtual std::string getInfo();

private:
	std::string m_uuid;

};

class StringMessage : public ExtendedDataMailboxMessage
{
public:
	StringMessage();
	StringMessage(const std::string& message);

	virtual ~StringMessage() {}

	virtual void Serialize();
	virtual void Deserialize();
	virtual std::string getInfo();

	std::string getMessage() const { return m_message; }


private:
	std::string m_message;
};



class WatchdogMessage : public ExtendedDataMailboxMessage
{
public:
	typedef enum : char
	{
		REGISTER_REQUEST = 0,
		REGISTER_REPLY,
		UNREGISTER_REQUEST,
		UNREGISTER_REPLY,
		UPDATE_SETTINGS,
		KICK,
		START,
		STOP,
		SYNC_REQUEST,
		SYNC_BROADCAST,
		TERMINATE_REQUEST,
		TERMINATE_BROADCAST,
		ANY,
		NONE
	} MessageClass;

	virtual void Serialize();
	virtual void Deserialize();
	virtual std::string getInfo();

	WatchdogMessage();

	WatchdogMessage(const std::string& name,
		const SlotSettings& settings,
		unsigned int PID,
		enuActionOnFailure onFailure = enuActionOnFailure::RESET_ONLY,
		MessageClass type = NONE);

	WatchdogMessage(const std::string& name,
		MessageClass type = NONE);

	WatchdogMessage(MessageClass type);

	std::string getName() const { return m_name; }

	MessageClass getMessageClass() const { return m_messageClass; }

	const SlotSettings& getSettings() const { return m_settings; }

	int getPID() const { return m_PID; }

	enuActionOnFailure getActionOnFailure() const { return m_onFailure; }

	static std::string getMessageClassName(MessageClass messageClass);
	std::string getMessageClassName() const { return getMessageClassName(m_messageClass); }

private:
	enuActionOnFailure m_onFailure;

	std::string m_name;

	MessageClass m_messageClass;

	SlotSettings m_settings;

	unsigned int m_PID;
};

#endif
