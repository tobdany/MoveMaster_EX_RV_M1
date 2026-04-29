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
			<Item Name="Ctrl_Array_Enc_Out.ctl" Type="VI" URL="../Control/Ctrl_Array_Enc_Out.ctl"/>
			<Item Name="Ctrl_Array_Enc_Out_final.ctl" Type="VI" URL="../Control/Ctrl_Array_Enc_Out_final.ctl"/>
			<Item Name="Ctrl_Articulacion_PIDGui.ctl" Type="VI" URL="../Control/Ctrl_Articulacion_PIDGui.ctl"/>
			<Item Name="Ctrl_ControlPIDEvent.ctl" Type="VI" URL="../Control/Ctrl_ControlPIDEvent.ctl"/>
			<Item Name="Ctrl_Dashboard_StateMach.ctl" Type="VI" URL="../Control/Ctrl_Dashboard_StateMach.ctl"/>
			<Item Name="Ctrl_DataLogger_SM.ctl" Type="VI" URL="../Control/Ctrl_DataLogger_SM.ctl"/>
			<Item Name="Ctrl_DataLoggerQueue.ctl" Type="VI" URL="../Control/Ctrl_DataLoggerQueue.ctl"/>
			<Item Name="Ctrl_Datos6Motores.ctl" Type="VI" URL="../Control/Ctrl_Datos6Motores.ctl"/>
			<Item Name="Ctrl_datos6motores_final.ctl" Type="VI" URL="../Control/Ctrl_datos6motores_final.ctl"/>
			<Item Name="Ctrl_DLQueue.ctl" Type="VI" URL="../Control/Ctrl_DLQueue.ctl"/>
			<Item Name="Ctrl_Enc_Values_Steps.ctl" Type="VI" URL="../Control/Ctrl_Enc_Values_Steps.ctl"/>
			<Item Name="Ctrl_Errores.ctl" Type="VI" URL="../Control/Ctrl_Errores.ctl"/>
			<Item Name="Ctrl_EscrituraComandos.ctl" Type="VI" URL="../Control/Ctrl_EscrituraComandos.ctl"/>
			<Item Name="Ctrl_EstadoMotorEsp.ctl" Type="VI" URL="../Ctrl_EstadoMotorEsp.ctl"/>
			<Item Name="Ctrl_Estados.ctl" Type="VI" URL="../Ctrl_Estados.ctl"/>
			<Item Name="Ctrl_EstadosMotor.ctl" Type="VI" URL="../Ctrl_EstadosMotor.ctl"/>
			<Item Name="Ctrl_Fgv_DatosMotores.ctl" Type="VI" URL="../Control/Ctrl_Fgv_DatosMotores.ctl"/>
			<Item Name="Ctrl_GuiEvent.ctl" Type="VI" URL="../Control/Ctrl_GuiEvent.ctl"/>
			<Item Name="Ctrl_IndicadoresControl.ctl" Type="VI" URL="../Control/Ctrl_IndicadoresControl.ctl"/>
			<Item Name="Ctrl_IndicadoresMainPanel.ctl" Type="VI" URL="../Control/Ctrl_IndicadoresMainPanel.ctl"/>
			<Item Name="Ctrl_MainPanelGui.ctl" Type="VI" URL="../Ctrl_MainPanelGui.ctl"/>
			<Item Name="Ctrl_Modbus13.ctl" Type="VI" URL="../Control/Ctrl_Modbus13.ctl"/>
			<Item Name="Ctrl_Modbus28.ctl" Type="VI" URL="../Control/Ctrl_Modbus28.ctl"/>
			<Item Name="Ctrl_Modbus64.ctl" Type="VI" URL="../Control/Ctrl_Modbus64.ctl"/>
			<Item Name="Ctrl_Modbus_To_Esp32.ctl" Type="VI" URL="../Control/Ctrl_Modbus_To_Esp32.ctl"/>
			<Item Name="Ctrl_Motores_Valores.ctl" Type="VI" URL="../Control/Ctrl_Motores_Valores.ctl"/>
			<Item Name="Ctrl_QueuePID.ctl" Type="VI" URL="../Control/Ctrl_QueuePID.ctl"/>
		</Item>
		<Item Name="Modbus_VI" Type="Folder">
			<Item Name="decodeModbusMessage.vi" Type="VI" URL="../decodeModbusMessage.vi"/>
		</Item>
		<Item Name="Subpanel" Type="Folder">
			<Item Name="Control_ControPIDEvent.vi" Type="VI" URL="../Control_ControPIDEvent.vi"/>
			<Item Name="Control_Event.vi" Type="VI" URL="../Control_Event.vi"/>
			<Item Name="Control_VI.vi" Type="VI" URL="../Control_VI.vi"/>
			<Item Name="ControlPID_VI.vi" Type="VI" URL="../ControlPID_VI.vi"/>
			<Item Name="Dashboard.vi" Type="VI" URL="../Dashboard.vi"/>
			<Item Name="Dashboard_event.vi" Type="VI" URL="../Dashboard_event.vi"/>
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
		<Item Name="calculoVelocidad.vi" Type="VI" URL="../calculoVelocidad.vi"/>
		<Item Name="Cluster_ControlPID_Subpanel.ctl" Type="VI" URL="../Control/Cluster_ControlPID_Subpanel.ctl"/>
		<Item Name="Cluster_Dashboard.ctl" Type="VI" URL="../Control/Cluster_Dashboard.ctl"/>
		<Item Name="Cluster_EstadoMotores.ctl" Type="VI" URL="../Cluster_EstadoMotores.ctl"/>
		<Item Name="Cluster_ValorFinalesCarrera.ctl" Type="VI" URL="../Cluster_ValorFinalesCarrera.ctl"/>
		<Item Name="Control 2" Type="VI"/>
		<Item Name="Controlador_grados_a_pasos.vi" Type="VI" URL="../Controlador_grados_a_pasos.vi"/>
		<Item Name="Ctrl_CSVLoop.ctl" Type="VI" URL="../Ctrl_CSVLoop.ctl"/>
		<Item Name="Enum_VICaller.ctl" Type="VI" URL="../Control/VI_Caller/Enum_VICaller.ctl"/>
		<Item Name="escribirCSV.vi" Type="VI" URL="../escribirCSV.vi"/>
		<Item Name="esP32DataToInfo.vi" Type="VI" URL="../esP32DataToInfo.vi"/>
		<Item Name="excel_creacion_archivo.vi" Type="VI" URL="../excel_creacion_archivo.vi"/>
		<Item Name="excel_lectura_archivo.vi" Type="VI" URL="../excel_lectura_archivo.vi"/>
		<Item Name="FGV_ADQ.vi" Type="VI" URL="../FGV_ADQ.vi"/>
		<Item Name="FGV_DatosMotores.vi" Type="VI" URL="../FGV_DatosMotores.vi"/>
		<Item Name="FGV_Error.vi" Type="VI" URL="../FGV_Error.vi"/>
		<Item Name="FGV_EstadoMotores.vi" Type="VI" URL="../FGV_EstadoMotores.vi"/>
		<Item Name="FGV_LoopCSV.vi" Type="VI" URL="../FGV_LoopCSV.vi"/>
		<Item Name="FGV_MainPanel.vi" Type="VI" URL="../FGV_MainPanel.vi"/>
		<Item Name="Global_init.vi" Type="VI" URL="../Global_init.vi"/>
		<Item Name="LogData_Command.vi" Type="VI" URL="../LogData_Command.vi"/>
		<Item Name="main.vi" Type="VI" URL="../main.vi"/>
		<Item Name="MainQueue_Analyzer.vi" Type="VI" URL="../MainQueue_Analyzer.vi"/>
		<Item Name="Muestreo.vi" Type="VI" URL="../GlobalVariable/Muestreo.vi"/>
		<Item Name="s.ctl" Type="VI" URL="../s.ctl"/>
		<Item Name="Selector_VI.ctl" Type="VI" URL="../Selector_VI.ctl"/>
		<Item Name="VI_caller.vi" Type="VI" URL="../VI_caller.vi"/>
		<Item Name="VI_opener.vi" Type="VI" URL="../VI_opener.vi"/>
		<Item Name="visacleaner.vi" Type="VI" URL="../../../../../../Users/danie/Downloads/visacleaner.vi"/>
		<Item Name="Dependencies" Type="Dependencies"/>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
