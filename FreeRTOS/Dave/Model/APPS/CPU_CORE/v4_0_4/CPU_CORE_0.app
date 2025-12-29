<?xml version="1.0" encoding="ASCII"?>
<ResourceModel:App xmi:version="2.0" xmlns:xmi="http://www.omg.org/XMI" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ResourceModel="http://www.infineon.com/Davex/Resource.ecore" name="CPU_CORE" URI="http://resources/4.0.4/app/CPU_CORE/0" description="This APP configures specific CPU core. Configuration can be done by signal connections and GUI parameters" version="4.0.4" minDaveVersion="4.2.4" instanceLabel="CPU_CORE_0" appLabel="">
  <upwardMapList xsi:type="ResourceModel:RequiredApp" href="../../FREERTOS_AURIX/v4_0_4/FREERTOS_AURIX_0.app#//@requiredApps.1"/>
  <properties provideInit="true" sharable="true"/>
  <virtualSignals name="INT" URI="http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_global" hwSignal="global" hwResource="//@hwResources.0" visible="true">
    <upwardMapList xsi:type="ResourceModel:Connections" href="../../FREERTOS_AURIX/v4_0_4/FREERTOS_AURIX_0.app#//@connections.1"/>
    <upwardMapList xsi:type="ResourceModel:Connections" href="../../FREERTOS_AURIX/v4_0_4/FREERTOS_AURIX_0.app#//@connections.2"/>
    <upwardMapList xsi:type="ResourceModel:Connections" href="../../FREERTOS_AURIX/v4_0_4/FREERTOS_AURIX_0.app#//@connections.4"/>
    <upwardMapList xsi:type="ResourceModel:Connections" href="../../FREERTOS_AURIX/v4_0_4/FREERTOS_AURIX_0.app#//@connections.5"/>
  </virtualSignals>
  <virtualSignals name="ISPIR" URI="http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_ispir" hwSignal="ispir" hwResource="//@hwResources.0"/>
  <virtualSignals name="ISPIR" URI="http://resources/4.0.4/app/CPU_CORE/0/vs_ir_icu_ispir" hwSignal="ispir" hwResource="//@hwResources.2"/>
  <virtualSignals name="global" URI="http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_mpu_global" hwSignal="global" hwResource="//@hwResources.1"/>
  <hwResources name="CPU Core" URI="http://resources/4.0.4/app/CPU_CORE/0/hwres_cpu_core" resourceGroupUri="peripheral/cpu/*/core" mResGrpUri="peripheral/cpu/*/core">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/CPU0/CPU0_0.dd#//@provided.0"/>
  </hwResources>
  <hwResources name="CPU Memory Protection Unit" URI="http://resources/4.0.4/app/CPU_CORE/0/hwres_cpu_mpu" resourceGroupUri="peripheral/cpu/*/memory_protection" mResGrpUri="peripheral/cpu/*/memory_protection">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/CPU0/CPU0_0.dd#//@provided.2"/>
  </hwResources>
  <hwResources name="Interrupt Control Unit" URI="http://resources/4.0.4/app/CPU_CORE/0/hwres_ir_icu" resourceGroupUri="peripheral/int/0/icu/*" mResGrpUri="peripheral/int/0/icu/*">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/INT0/INT0_0.dd#//@provided.3"/>
  </hwResources>
  <connections URI="http://resources/4.0.4/app/CPU_CORE/0/http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_ispir/http://resources/4.0.4/app/CPU_CORE/0/vs_ir_icu_ispir" systemDefined="true" sourceSignal="ISPIR" targetSignal="ISPIR" srcVirtualSignal="//@virtualSignals.1" targetVirtualSignal="//@virtualSignals.2"/>
  <connections URI="http://resources/4.0.4/app/CPU_CORE/0/http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_global/http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_mpu_global" systemDefined="true" sourceSignal="INT" targetSignal="global" srcVirtualSignal="//@virtualSignals.0" targetVirtualSignal="//@virtualSignals.3"/>
</ResourceModel:App>
