# Example 01 — minimal empty program

$(call make-bin,tn_example_01_empty_c,tn_example_01_empty,,-ltn_sdk)

# Example 02 — account lifecycle: create / increment / resize / delete

$(call make-bin,tn_example_02_counter_c,tn_example_02_counter,,-ltn_sdk)

# Example 03 — page-fault cost experiment

$(call make-bin,tn_example_03_storage_c,tn_example_03_storage,,-ltn_sdk)

# Example 04 — SHA-256: portable C (arm A) vs Zknh instructions (arm B)

$(call make-bin,tn_example_04_hash_a_c,tn_example_04_hash_a,,-ltn_sdk)
$(call make-bin,tn_example_04_hash_b_c,tn_example_04_hash_b,,-ltn_sdk)

# Example 05 — event/stack page charge experiment (one binary, 4 instructions)

$(call make-bin,tn_example_05_events_c,tn_example_05_events,,-ltn_sdk)

# Example 06 — direct measurement of the instruction term

$(call make-bin,tn_example_06_instructions_c,tn_example_06_instructions,,-ltn_sdk)

# Example 07 — cross-program invocation pricing

$(call make-bin,tn_example_07_callee_c,tn_example_07_callee,,-ltn_sdk)
$(call make-bin,tn_example_07_caller_c,tn_example_07_caller,,-ltn_sdk)
