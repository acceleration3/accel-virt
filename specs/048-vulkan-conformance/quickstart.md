# Quickstart: the conformance run (048)

## Without the pinned CTS - what is proven here

```bash
ctest --test-dir build -R 'tools_test_cts|tools_test_compare_cts|tools_test_fetch_cts'
python3 -B tools/compare_cts.py /dev/null /dev/null   # exit 2: nothing to compare
python3 -B tools/guest/run_guest_suite.py --cts --image <image>   # refused, names the fetch
```

Every class that decides a verdict is driven over `.qpa` logs the tests write,
including a log that ends inside a case, a status nobody defines and a name
with a newline in it.

## With the CTS fetched

```bash
python3 -B tools/fetch_cts.py                # fills third_party/vulkan-cts/
python3 -B tools/guest/run_guest_suite.py --image <image> --cts \
        --executor build-qemu/librvk_executor.so
```

The run pushes the CTS and one shard's case list at a time, pulls each
`TestResults.qpa`, and leaves them in the work directory beside `diag/`.

Then, on the host, the same subset natively:

```bash
third_party/vulkan-cts/deqp-vk --deqp-caselist-file=tools/cts_subset.txt \
        --deqp-log-filename=host.qpa
python3 -B tools/compare_cts.py <work>/cts/guest.qpa host.qpa
python3 -B tools/check_cts_ratchet.py <work>/cts/guest.qpa
```

The comparison is the verdict; the ratchet is what keeps it from sliding.
