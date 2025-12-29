<?xml version="1.0" encoding="ASCII"?>
<ResourceModel:App xmi:version="2.0" xmlns:xmi="http://www.omg.org/XMI" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ResourceModel="http://www.infineon.com/Davex/Resource.ecore" name="FREERTOS_AURIX" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0" description="FreeRTOS is an open source real-time operating system (RTOS) for embedded systems" version="4.0.4" minDaveVersion="4.2.4" instanceLabel="FREERTOS_AURIX_0" appLabel="" containingProxySignal="true">
  <properties singleton="true" provideInit="true" sharable="true"/>
  <virtualSignals name="TICK_INT" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_stm_sr0" hwSignal="sr0_int" hwResource="//@hwResources.0"/>
  <virtualSignals name="SW_INT" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_gpsr_trig" hwSignal="trig0" hwResource="//@hwResources.1" visible="true"/>
  <virtualSignals name="TICK_INT req" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_0_in" hwSignal="in" hwResource="//@hwResources.2"/>
  <virtualSignals name="TICK_INT" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_0_out" hwSignal="out" hwResource="//@hwResources.2"/>
  <virtualSignals name="SW_INT req" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_1_in" hwSignal="in" hwResource="//@hwResources.4"/>
  <virtualSignals name="SW_INT" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_1_out" hwSignal="out" hwResource="//@hwResources.4" visible="true"/>
  <virtualSignals name="global" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_prio_0_global" hwSignal="global" hwResource="//@hwResources.3"/>
  <virtualSignals name="global" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_prio_1_global" hwSignal="global" hwResource="//@hwResources.5"/>
  <requiredApps URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/appres_clock" requiredAppName="CLOCK" requiringMode="SHARABLE">
    <downwardMapList xsi:type="ResourceModel:App" href="../../CLOCK/v4_0_4/CLOCK_0.app#/"/>
  </requiredApps>
  <requiredApps URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/appres_cpu" requiredAppName="CPU_CORE" requiringMode="SHARABLE">
    <downwardMapList xsi:type="ResourceModel:App" href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#/"/>
  </requiredApps>
  <requiredApps URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/appres_timer_stm" requiredAppName="TIMER_STM" required="false" requiringMode="SHARABLE"/>
  <requiredApps URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/appres_timer_gtm" requiredAppName="TIMER_GTM" required="false" requiringMode="SHARABLE"/>
  <hwResources name="STM Channel" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/hwres_stm_channel" resourceGroupUri="peripheral/stm/*/common" mResGrpUri="peripheral/stm/*/common">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/STM0/STM0_0.dd#//@provided.0"/>
  </hwResources>
  <hwResources name="General Purpose Service Request" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/hwres_int_gpsr" resourceGroupUri="peripheral/int/0/gpsr/*" mResGrpUri="peripheral/int/0/gpsr/*">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/INT0/INT0_0.dd#//@provided.7"/>
  </hwResources>
  <hwResources name="Tick Interrupt SRN" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/hwres_int_srn_0" resourceGroupUri="peripheral/int/0/srn/*" mResGrpUri="peripheral/int/0/srn/*">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/INT0/INT0_0.dd#//@provided.4"/>
  </hwResources>
  <hwResources name="Tick Interrupt" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/hwres_cpu_prio_0" resourceGroupUri="peripheral/cpu/*/prio/sv0" solverVariable="true" mResGrpUri="peripheral/cpu/*/prio/sv0">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/CPU0/CPU0_0.dd#//@provided.1"/>
    <solverVarMap index="4">
      <value variableName="sv0" solverValue="2"/>
    </solverVarMap>
    <solverVarMap index="4">
      <value variableName="sv0" solverValue="2"/>
    </solverVarMap>
  </hwResources>
  <hwResources name="SW Interrupt SRN" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/hwres_int_srn_1" resourceGroupUri="peripheral/int/0/srn/*" mResGrpUri="peripheral/int/0/srn/*">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/INT0/INT0_0.dd#//@provided.0"/>
  </hwResources>
  <hwResources name="SW Interrupt" URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/hwres_cpu_prio_1" resourceGroupUri="peripheral/cpu/*/prio/sv1" solverVariable="true" mResGrpUri="peripheral/cpu/*/prio/sv1">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/CPU0/CPU0_0.dd#//@provided.3"/>
    <solverVarMap index="4">
      <value variableName="sv1" solverValue="1"/>
    </solverVarMap>
    <solverVarMap index="4">
      <value variableName="sv1" solverValue="1"/>
    </solverVarMap>
  </hwResources>
  <connections URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_gpsr_trig/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_1_in" systemDefined="true" sourceSignal="SW_INT" targetSignal="SW_INT req" srcVirtualSignal="//@virtualSignals.1" targetVirtualSignal="//@virtualSignals.4"/>
  <connections URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_1_out/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_core_global" systemDefined="true" sourceSignal="SW_INT" targetSignal="INT" srcVirtualSignal="//@virtualSignals.5" proxyTargetVirtualSignalUri="http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_global" containingProxySignal="true">
    <downwardMapList xsi:type="ResourceModel:VirtualSignal" href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
    <targetVirtualSignal href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
  </connections>
  <connections URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_core_global/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_prio_1_global" systemDefined="true" sourceSignal="INT" targetSignal="global" targetVirtualSignal="//@virtualSignals.7" proxySrcVirtualSignalUri="http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_global" containingProxySignal="true">
    <downwardMapList xsi:type="ResourceModel:VirtualSignal" href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
    <srcVirtualSignal href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
  </connections>
  <connections URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_stm_sr0/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_0_in" systemDefined="true" sourceSignal="TICK_INT" targetSignal="TICK_INT req" srcVirtualSignal="//@virtualSignals.0" targetVirtualSignal="//@virtualSignals.2"/>
  <connections URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_int_srn_0_out/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_core_global" systemDefined="true" sourceSignal="TICK_INT" targetSignal="INT" srcVirtualSignal="//@virtualSignals.3" proxyTargetVirtualSignalUri="http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_global" containingProxySignal="true">
    <downwardMapList xsi:type="ResourceModel:VirtualSignal" href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
    <targetVirtualSignal href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
  </connections>
  <connections URI="http://resources/4.0.4/app/FREERTOS_AURIX/0/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_core_global/http://resources/4.0.4/app/FREERTOS_AURIX/0/vs_cpu_prio_0_global" systemDefined="true" sourceSignal="INT" targetSignal="global" targetVirtualSignal="//@virtualSignals.6" proxySrcVirtualSignalUri="http://resources/4.0.4/app/CPU_CORE/0/vs_cpu_core_global" containingProxySignal="true">
    <downwardMapList xsi:type="ResourceModel:VirtualSignal" href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
    <srcVirtualSignal href="../../CPU_CORE/v4_0_4/CPU_CORE_0.app#//@virtualSignals.0"/>
  </connections>
</ResourceModel:App>
