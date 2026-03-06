<?xml version='1.0' encoding='UTF-8'?>
<Project Type="Project" LVVersion="26008000">
	<Property Name="NI.LV.All.SaveVersion" Type="Str">26.0</Property>
	<Property Name="NI.LV.All.SourceOnly" Type="Bool">true</Property>
	<Item Name="My Computer" Type="My Computer">
		<Property Name="server.app.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.control.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.tcp.enabled" Type="Bool">false</Property>
		<Property Name="server.tcp.port" Type="Int">0</Property>
		<Property Name="server.tcp.serviceName" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.tcp.serviceName.default" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.vi.callsEnabled" Type="Bool">true</Property>
		<Property Name="server.vi.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="specify.custom.address" Type="Bool">false</Property>
		<Item Name="Controls" Type="Folder">
			<Item Name="Ctr_Enc_Values.ctl" Type="VI" URL="../Control/Ctr_Enc_Values.ctl"/>
			<Item Name="Ctrl_DataLoggerQueue.ctl" Type="VI" URL="../Control/Ctrl_DataLoggerQueue.ctl"/>
			<Item Name="Ctrl_Datos6Motores.ctl" Type="VI" URL="../Control/Ctrl_Datos6Motores.ctl"/>
			<Item Name="Ctrl_DLQueue.ctl" Type="VI" URL="../Control/Ctrl_DLQueue.ctl"/>
			<Item Name="Ctrl_Enc_Values_Steps.ctl" Type="VI" URL="../Control/Ctrl_Enc_Values_Steps.ctl"/>
			<Item Name="Ctrl_Errores.ctl" Type="VI" URL="../Control/Ctrl_Errores.ctl"/>
			<Item Name="Ctrl_Estados.ctl" Type="VI" URL="../Ctrl_Estados.ctl"/>
			<Item Name="Ctrl_IndicadoresMainPanel.ctl" Type="VI" URL="../Control/Ctrl_IndicadoresMainPanel.ctl"/>
			<Item Name="Ctrl_Modbus13.ctl" Type="VI" URL="../Control/Ctrl_Modbus13.ctl"/>
			<Item Name="Ctrl_Modbus64.ctl" Type="VI" URL="../Control/Ctrl_Modbus64.ctl"/>
			<Item Name="Ctrl_Motores_Valores.ctl" Type="VI" URL="../Control/Ctrl_Motores_Valores.ctl"/>
		</Item>
		<Item Name="Modbus_VI" Type="Folder">
			<Item Name="decodeModbusMessage.vi" Type="VI" URL="../decodeModbusMessage.vi"/>
		</Item>
		<Item Name="Unused" Type="Folder">
			<Item Name="Decode_Modbus_Message.vi" Type="VI" URL="../utils/Decode_Modbus_Message.vi"/>
			<Item Name="openVISA_current.vi" Type="VI" URL="../utils/openVISA_current.vi"/>
		</Item>
		<Item Name="VirtualInstruments" Type="Folder">
			<Item Name="actualizarIndicadores.vi" Type="VI" URL="../actualizarIndicadores.vi"/>
			<Item Name="manejadorErrores.vi" Type="VI" URL="../manejadorErrores.vi"/>
			<Item Name="WriteCommand.vi" Type="VI" URL="../WriteCommand.vi"/>
		</Item>
		<Item Name="Ctrl_DataLogger_SM.ctl" Type="VI" URL="../Control/Ctrl_DataLogger_SM.ctl"/>
		<Item Name="Ctrl_EscrituraComandos.ctl" Type="VI" URL="../Control/Ctrl_EscrituraComandos.ctl"/>
		<Item Name="Ctrl_Fgv_DatosMotores.ctl" Type="VI" URL="../Control/Ctrl_Fgv_DatosMotores.ctl"/>
		<Item Name="FGV_ADQ.vi" Type="VI" URL="../FGV_ADQ.vi"/>
		<Item Name="FGV_DatosMotores.vi" Type="VI" URL="../FGV_DatosMotores.vi"/>
		<Item Name="FGV_Error.vi" Type="VI" URL="../FGV_Error.vi"/>
		<Item Name="LogData_Command.vi" Type="VI" URL="../LogData_Command.vi"/>
		<Item Name="main.vi" Type="VI" URL="../main.vi"/>
		<Item Name="MainQueue_Analyzer.vi" Type="VI" URL="../MainQueue_Analyzer.vi"/>
		<Item Name="Dependencies" Type="Dependencies"/>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
