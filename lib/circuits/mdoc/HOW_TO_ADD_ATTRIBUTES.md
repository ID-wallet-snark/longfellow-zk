# Adding New Attributes to ZK Circuit

To verify a specific attribute (e.g., `driving_privileges`), the following components are required:
1.  **Attribute Definition**: Registration in `mdoc_attribute_ids.h`.
2.  **Signed Data**: An mDL containing the attribute, signed by a trusted issuer.
3.  **Client Logic**: Request logic implementation in the prover/verifier workflow.

## 1. Register Attribute
Ensure the attribute is defined in `lib/circuits/mdoc/mdoc_attribute_ids.h`.

```cpp
constexpr MdocAttribute kMdocAttributes[] = {
    // ...
    {"driving_privileges", kMDLNamespace}, 
    {"new_attribute", kMDLNamespace}, 
};
```

## 2. Data Requirements (Critical)
Verification requires data signed by the issuer's private key. 

> [!IMPORTANT]  
> You cannot simply create arbitrary JSON/CBOR and verify it. The system enforces signature validity.

To test a new attribute, you must either:
*   Have access to the issuer's private key to sign a new mDL structure.
*   Use an existing mDL that already contains the desired attribute.

*Note: This repository uses pre-signed mock data (`mdoc_examples.h`) because private keys are not public. We successfully verified the `height` attribute because it was present in our pre-signed test vector.*

## 3. Implementation
Define the `RequestedAttribute` struct with the expected CBOR value.

Example for `driving_privileges` (Category B):

```cpp
static const RequestedAttribute driving_privileges_B = {
    .namespace_id = { ... }, // org.iso.18013.5.1
    .id = {'d', 'r', 'i', 'v', 'i', 'n', 'g', ...},
    // Exact CBOR bytes matching the signed mDL content
    .cbor_value = { ... }, 
    .verification_type = 0 // Equality check
};
```

Pass this to `run_mdoc_prover` and `run_mdoc_verifier`. The prover will generate a ZK proof that the signed mDL contains this specific attribute value without revealing the full document signature.
