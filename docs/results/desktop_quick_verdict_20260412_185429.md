# Desktop Quick Verdict (20260412_185429)

- deterministic: FAIL (matrix_slo=FAIL, soak_slo=PASS)
  - matrix max: kv_put=29062 ts_insert=42458 rel_insert=29186 rel_delete=28927 txn_commit=45251 compact=24444 reopen=64245 spikes_gt_5ms=751 fail_count=0
  - soak line: deterministic,80000,34702,24169,34057,32768,34461,20020,51178,64143,4341,0,1

- balanced: PASS (matrix_slo=PASS, soak_slo=PASS)
  - matrix max: kv_put=34498 ts_insert=34235 rel_insert=36661 rel_delete=31220 txn_commit=34376 compact=27797 reopen=61031 spikes_gt_5ms=938 fail_count=0
  - soak line: balanced,120000,40444,38196,44720,38302,31868,24572,73484,96158,6577,0,1

- stress: PASS (matrix_slo=PASS, soak_slo=PASS)
  - matrix max: kv_put=42971 ts_insert=42763 rel_insert=42080 rel_delete=41474 txn_commit=58996 compact=35416 reopen=83603 spikes_gt_5ms=1173 fail_count=0
  - soak line: stress,150000,45769,46514,44483,44112,77341,30338,71821,120164,8286,0,1
