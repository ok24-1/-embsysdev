*** Settings ***
Library   String
Library   SerialLibrary

*** Variables ***
${com}   	COM4
${baud} 	115200
${board}	nRF
${seq}      RGYX

*** Test Cases ***
Connect Serial
	Log To Console  Connecting to ${board}
	Add Port  ${com}  baudrate=${baud}  encoding=ascii  timeout=30
	Port Should Be Open  ${com}
	Reset Input Buffer
	Reset Output Buffer

Serial Led Control
	# lähetetään RYG ja lopetusmerkki X
	Reset Input Buffer
	Send UART Message   ${seq}
	Log To Console   Send sequence ${seq}

	# vastaanotetaan merkkijono kunnes lopetusmerkki X
	${read} =   Read Until   terminator=X   encoding=ascii   terminator_encoding=ascii

	# konsolille näkyviin vastaanotettu merkkijono
	Log To Console   Received ${read}
	Log Sequence Duration   ${read}

Time Length Error
	Reset Input Buffer
	Send UART Message   12345X
	${read} =   Read Until   terminator=X   encoding=ascii   terminator_encoding=ascii
	Should Contain   ${read}   -1X

Time Value Error
	Reset Input Buffer
	Send UART Message   12A456X
	${read} =   Read Until   terminator=X   encoding=ascii   terminator_encoding=ascii
	Should Contain   ${read}   -3X

Time Format Error
	Reset Input Buffer
	Send UART Message   246000X
	${read} =   Read Until   terminator=X   encoding=ascii   terminator_encoding=ascii
	Should Contain   ${read}   -4X


Test RYGRYGRYG
	Reset Input Buffer
	Send UART Message   RYGRYGRYGX
	${read} =   Read Until   terminator=X   encoding=ascii   terminator_encoding=ascii
	Log Sequence Duration   ${read}

Test D Starts Sequence Timing
	Reset Input Buffer
	Send UART Message   DRX
	${read} =   Read Until   terminator=X   encoding=ascii   terminator_encoding=ascii
	Log Sequence Duration   ${read}

Test Invalid Traffic Light Sequence
	Reset Input Buffer
	Send UART Message   RYGRYGRYQX
	${read} =   Read Until   terminator=X   encoding=ascii   terminator_encoding=ascii
	Should Contain   ${read}   -3X

Disconnect Serial
	Log To Console  Disconnecting ${board}
	[TearDown]  Delete Port  ${com}

*** Keywords ***
Log Sequence Duration
	[Arguments]   ${response}
	${duration} =   Get Regexp Matches   ${response}   [0-9]+ms
	Should Not Be Empty   ${duration}
	Log To Console   Sequence duration: ${duration}[0]

Send UART Message
	[Arguments]   ${message}
	${length} =   Get Length   ${message}
	FOR   ${index}   IN RANGE   ${length}
		${end} =   Evaluate   ${index} + 1
		${character} =   Get Substring   ${message}   ${index}   ${end}
		Write Data   ${character}   encoding=ascii
		Sleep   0.02s
	END


	
	
	