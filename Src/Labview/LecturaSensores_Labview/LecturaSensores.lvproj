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
		</Item>
		<Item Name="Modbus_VI" Type="Folder">
			<Item Name="decodeModbusMessage.vi" Type="VI" URL="../decodeModbusMessage.vi"/>
		</Item>
		<Item Name="actualizarIndicadores.vi" Type="VI" URL="../actualizarIndicadores.vi"/>
		<Item Name="Check_Modbus_Packet_Header.vi" Type="VI" URL="../Check_Modbus_Packet_Header.vi"/>
		<Item Name="Ctrl_Enc_Values_Steps.ctl" Type="VI" URL="../Control/Ctrl_Enc_Values_Steps.ctl"/>
		<Item Name="Ctrl_Errores.ctl" Type="VI" URL="../Control/Ctrl_Errores.ctl"/>
		<Item Name="cualControl.ctl" Type="VI" URL="../Control/cualControl.ctl"/>
		<Item Name="Decode_Modbus_Message.vi" Type="VI" URL="../utils/Decode_Modbus_Message.vi"/>
		<Item Name="main.vi" Type="VI" URL="../main.vi"/>
		<Item Name="manejadorErrores.vi" Type="VI" URL="../manejadorErrores.vi"/>
		<Item Name="openVISA_current.vi" Type="VI" URL="../utils/openVISA_current.vi"/>
		<Item Name="readCurrent.vi" Type="VI" URL="../readCurrent.vi"/>
		<Item Name="StateEnum.ctl" Type="VI" URL="../StateEnum.ctl"/>
		<Item Name="Verify_Modbus_CRC.vi" Type="VI" URL="../Verify_Modbus_CRC.vi"/>
		<Item Name="Dependencies" Type="Dependencies"/>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
