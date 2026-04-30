import json
import uuid
import sys
import glob

def fix_rule_chain_uuids(file_path):
    with open(file_path, 'r') as f:
        data = json.load(f)
        
    def replace_config_uuids(obj):
        if isinstance(obj, dict):
            # Check for placeholder rule chain IDs in configuration
            if "ruleChainId" in obj:
                if "REPLACE_WITH" in str(obj["ruleChainId"]):
                    obj["ruleChainId"] = str(uuid.uuid4())
            for k, v in obj.items():
                replace_config_uuids(v)
        elif isinstance(obj, list):
            for item in obj:
                replace_config_uuids(item)
                
    replace_config_uuids(data)
    
    with open(file_path, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"Fixed {file_path}")

if __name__ == "__main__":
    for file_path in glob.glob("e:/New folder (7)/Downloads/CoolCycle-thingsboard/architecture/tb_export/*.json"):
        fix_rule_chain_uuids(file_path)
