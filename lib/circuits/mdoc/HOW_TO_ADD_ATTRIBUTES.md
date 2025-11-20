# How to Add New Attributes to the ZK Circuit

This guide explains how to add and verify a new attribute (e.g., `driving_privileges`) in the Zero-Knowledge Proof system.

## Prerequisites

To verify a specific attribute, you need:
1.  **The Attribute Definition**: It must be recognized by the system (in `mdoc_attribute_ids.h`).
2.  **Signed Data**: An mDL (Mobile Driving License) signed by a valid issuer that *contains* this attribute.
3.  **A Request**: Code to request this specific attribute during proof generation.

## Step-by-Step Guide

### 1. Define the Attribute (if missing)
Check `lib/circuits/mdoc/mdoc_attribute_ids.h`. If your attribute isn't listed, add it:

```cpp
// lib/circuits/mdoc/mdoc_attribute_ids.h
constexpr MdocAttribute kMdocAttributes[] = {
    // ... existing attributes ...
    {"driving_privileges", kMDLNamespace}, // Already exists!
    {"my_new_attribute", kMDLNamespace},   // Add yours here
};
```

### 2. Create Signed mDL Data
> [!IMPORTANT]
> This is the most critical step. You cannot verify data that hasn't been signed by the issuer.

You need to generate an mDL structure (CBOR) containing your new attribute and **sign it using the Issuer's Private Key**.

Example structure for `driving_privileges`:
```cbor
{
  "org.iso.18013.5.1": {
    "driving_privileges": [
      {
        "vehicle_category_code": "B",
        "issue_date": "2024-01-01",
        "expiry_date": "2034-01-01"
      }
    ]
  }
}
```

*Note: In this codebase, we use pre-signed mock data (`mdoc_examples.h`) because we don't have the private keys to generate new valid signatures.*

### 3. Define the Request in C++
In your test or application code (e.g., `french_license_test.cc`), define what you want to prove.

To prove the user has "Permis B" (Category B), you would request the `driving_privileges` attribute and provide the expected CBOR value.

```cpp
// Define the expected CBOR value for "Category B"
// This depends on the exact CBOR encoding of the attribute in the mDL.
static const RequestedAttribute driving_privileges_B = {
    .namespace_id = {'o', 'r', 'g', '.', 'i', 's', 'o', '.', '1', '8', '0', '1', '3', '.', '5', '.', '1'},
    .id = {'d', 'r', 'i', 'v', 'i', 'n', 'g', '_', 'p', 'r', 'i', 'v', 'i', 'l', 'e', 'g', 'e', 's'},
    // The CBOR bytes for the expected value (e.g., array containing map with category "B")
    .cbor_value = { ... bytes representing Category B ... }, 
    .namespace_len = 17,
    .id_len = 18,
    .cbor_value_len = ... // length of cbor_value
};
```

### 4. Run the Proof
Pass this new `RequestedAttribute` to the prover and verifier:

```cpp
RequestedAttribute attributes[] = {
    driving_privileges_B
};

// Run Prover
run_mdoc_prover(..., attributes, 1, ...);

// Run Verifier
run_mdoc_verifier(..., attributes, 1, ...);
```

## Why we used `height` instead
Since we cannot perform **Step 2** (Generate Signed Data), we had to use an attribute that *already exists* in the signed mock data (`height`) to demonstrate the verification flow. The cryptographic process (Selective Disclosure) is identical.

### Why can't we sign new data?
The `mdoc_examples.h` file contains **Public Keys** (`kIssuerPKX`, `kIssuerPKY`) used to *verify* signatures, but the corresponding **Private Keys** required to *create* signatures are not included in this open-source repository for security and privacy reasons. Without these private keys, any new data we generate would fail the signature verification step.
