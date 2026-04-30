import json
import glob
import os

def convert_to_pe_format(file_path):
    with open(file_path, 'r') as f:
        data = json.load(f)
    
    # Define standard configuration versions based on working examples
    version_map = {
        "org.thingsboard.rule.engine.filter.TbMsgTypeSwitchNode": 0,
        "org.thingsboard.rule.engine.telemetry.TbMsgTimeseriesNode": 1,
        "org.thingsboard.rule.engine.telemetry.TbMsgAttributesNode": 3,
        "org.thingsboard.rule.engine.flow.TbRuleChainInputNode": 1,
        "org.thingsboard.rule.engine.action.TbCreateAlarmNode": 0,
        "org.thingsboard.rule.engine.filter.TbScriptFilterNode": 0,
        "org.thingsboard.rule.engine.filter.TbJsFilterNode": 0,
        "org.thingsboard.rule.engine.metadata.TbGetAttributesNode": 1,
        "org.thingsboard.rule.engine.transform.TbTransformMsgNode": 0,
        "org.thingsboard.rule.engine.rest.TbRestApiCallNode": 3,
        "org.thingsboard.rule.engine.action.TbLogNode": 0,
        "org.thingsboard.rule.engine.rpc.TbSendRPCRequestNode": 0,
        "org.thingsboard.rule.engine.delay.TbMsgDelayNode": 0
    }

    # 1. Update ruleChain
    data["ruleChain"]["firstRuleNodeId"] = None
    data["ruleChain"]["configuration"] = None
    data["ruleChain"]["additionalInfo"] = None

    # 2. Update metadata
    old_nodes = data["metadata"]["nodes"]
    new_nodes = []
    
    # Layout logic: just space them out a bit
    x, y = 150, 150
    
    for i, node in enumerate(old_nodes):
        node_type = node.get("type")
        
        # Strip internal ThingsBoard IDs if they exist
        if "id" in node:
            del node["id"]
        
        # Add required PE metadata fields
        node["debugSettings"] = None
        node["singletonMode"] = False
        node["queueName"] = None
        node["configurationVersion"] = version_map.get(node_type, 0)
        
        # Add dummy layout info
        node["additionalInfo"] = {
            "description": "",
            "layoutX": x + (i % 3) * 250,
            "layoutY": y + (i // 3) * 100
        }
        
        # Special case: TbScriptFilterNode -> TbJsFilterNode in some PE versions?
        # The working example uses TbJsFilterNode with scriptLang: TBEL.
        if node_type == "org.thingsboard.rule.engine.filter.TbScriptFilterNode":
             node["type"] = "org.thingsboard.rule.engine.filter.TbJsFilterNode"
             # Copy script to tbelScript
             if "script" in node["configuration"]:
                 node["configuration"]["tbelScript"] = node["configuration"]["script"]
                 node["configuration"]["jsScript"] = "return true;" # fallback
        
        # Special case: TbCreateAlarmNode details
        if node_type == "org.thingsboard.rule.engine.action.TbCreateAlarmNode":
             if "alarmDetailsBuildJs" in node["configuration"]:
                 node["configuration"]["alarmDetailsBuildTbel"] = node["configuration"]["alarmDetailsBuildJs"]
                 # Standard JS details build
                 node["configuration"]["alarmDetailsBuildJs"] = "var details = {};\nif (metadata.prevAlarmDetails) {\n    details = JSON.parse(metadata.prevAlarmDetails);\n    delete metadata.prevAlarmDetails;\n}\nreturn details;"
             
             # Add missing mandatory PE fields
             node["configuration"]["useMessageAlarmData"] = False
             node["configuration"]["overwriteAlarmDetails"] = False
             node["configuration"]["propagateToOwnerHierarchy"] = True
             node["configuration"]["dynamicSeverity"] = False
             if "relationTypes" not in node["configuration"]:
                 node["configuration"]["relationTypes"] = []

        new_nodes.append(node)

    data["metadata"]["nodes"] = new_nodes
    data["metadata"]["version"] = 5
    data["metadata"]["firstNodeIndex"] = 0
    data["metadata"]["ruleChainConnections"] = None

    with open(file_path, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"Formatted {file_path} for ThingsBoard PE")

if __name__ == "__main__":
    files = [
        "rule_chain_root.json",
        "rule_chain_alarms.json",
        "rule_chain_whatsapp.json",
        "rule_chain_rpc.json"
    ]
    for filename in files:
        path = os.path.join("e:/New folder (7)/Downloads/CoolCycle-thingsboard/architecture/tb_export", filename)
        if os.path.exists(path):
            convert_to_pe_format(path)
