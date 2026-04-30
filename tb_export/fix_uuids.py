import json
import uuid
import sys
import glob
import os

def fix_rule_chain_uuids(file_path):
    with open(file_path, 'r') as f:
        data = json.load(f)
        
    id_map = {}
    
    # helper to replace id
    def replace_id(obj):
        if isinstance(obj, dict):
            if "entityType" in obj and obj["entityType"] == "RULE_NODE" and "id" in obj:
                old_id = obj["id"]
                if len(old_id) != 36: # Not a valid UUID
                    if old_id not in id_map:
                        id_map[old_id] = str(uuid.uuid4())
                    obj["id"] = id_map[old_id]
            
            for k, v in obj.items():
                replace_id(v)
        elif isinstance(obj, list):
            for item in obj:
                replace_id(item)
                
    replace_id(data)
    
    with open(file_path, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"Fixed {file_path}")

if __name__ == "__main__":
    for file_path in glob.glob("e:/New folder (7)/Downloads/CoolCycle-thingsboard/architecture/tb_export/*.json"):
        fix_rule_chain_uuids(file_path)
