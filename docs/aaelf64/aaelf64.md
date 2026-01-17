# ELF for the Arm® 64-bit Architecture (AArch64)

2024Q3

Date of Issue: 5th September 2024

![](_page_0_Picture_3.jpeg)

# <span id="page-1-0"></span>1 Preamble

# <span id="page-1-1"></span>1.1 ILP32 Beta

This document includes a beta proposal for ILP32 extensions to ELF for AArch64.

Feedback welcome through your normal channels.

# <span id="page-1-2"></span>1.2 Abstract

This document describes the use of the ELF binary file format in the Application Binary Interface (ABI) for the Arm 64-bit architecture.

# <span id="page-1-3"></span>1.3 Keywords

ELF, AArch64 ELF, ...

# <span id="page-1-4"></span>1.4 Latest release and defects report

Please check [Application Binary Interface for the Arm® Architecture](https://github.com/ARM-software/abi-aa) for the latest release of this document.

Please report defects in this specification to the [issue tracker page on GitHub.](https://github.com/ARM-software/abi-aa/issues)

# <span id="page-2-0"></span>1.5 Licence

This work is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License. To view a copy of this license, v[isit http://creativecommons.org/licenses/by-sa/4.0/](http://creativecommons.org/licenses/by-sa/4.0/) or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

Grant of Patent License. Subject to the terms and conditions of this license (both the Public License and this Patent License), each Licensor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable (except as stated in this section) patent license to make, have made, use, offer to sell, sell, import, and otherwise transfer the Licensed Material, where such license applies only to those patent claims licensable by such Licensor that are necessarily infringed by their contribution(s) alone or by combination of their contribution(s) with the Licensed Material to which such contribution(s) was submitted. If You institute patent litigation against any entity (including a cross-claim or counterclaim in a lawsuit) alleging that the Licensed Material or a contribution incorporated within the Licensed Material constitutes direct or contributory patent infringement, then any licenses granted to You under this license for that Licensed Material shall terminate as of the date such litigation is filed.

# <span id="page-2-1"></span>1.6 About the license

As identified more fully in the [Licence](#page-3-0) section, this project is licensed under CC-BY-SA-4.0 along with an additional patent license. The language in the additional patent license is largely identical to that in Apache-2.0 (specifically, Section 3 of Apache-2.0 as reflected at [https://www.apache.org/licenses/LICENSE-2.0\)](https://www.apache.org/licenses/LICENSE-2.0) with two exceptions.

First, several changes were made related to the defined terms so as to reflect the fact that such defined terms need to align with the terminology in CC-BY-SA-4.0 rather than Apache-2.0 (e.g., changing "Work" to "Licensed Material").

Second, the defensive termination clause was changed such that the scope of defensive termination applies to "any licenses granted to You" (rather than "any patent licenses granted to You"). This change is intended to help maintain a healthy ecosystem by providing additional protection to the community against patent litigation claims.

# <span id="page-2-2"></span>1.7 Contributions

Contributions to this project are licensed under an inbound=outbound model such that any such contributions are licensed by the contributor under the same terms as those in the [Licence](#page-3-0) section.

# <span id="page-2-3"></span>1.8 Trademark notice

The text of and illustrations in this document are licensed by Arm under a Creative Commons Attribution–Share Alike 4.0 International license ("CC-BY-SA-4.0"), with an additional clause on patents. The Arm trademarks featured here are registered trademarks or trademarks of Arm Limited (or its subsidiaries) in the US and/or elsewhere. All rights reserved. Please visit <https://www.arm.com/company/policies/trademarks>for more information about Arm's trademarks.

# <span id="page-2-4"></span>1.9 Copyright

Copyright (c) 2011, 2013, 2018-2024, Arm Limited and its affiliates. All rights reserved.

# Contents

<span id="page-3-3"></span><span id="page-3-2"></span><span id="page-3-1"></span><span id="page-3-0"></span>

| 1 Preamble     |                                                              | 2  |
|----------------|--------------------------------------------------------------|----|
|                | 1.1 ILP32 Beta                                               | 2  |
|                | 1.2 Abstract                                                 | 2  |
|                | 1.3 Keywords                                                 | 2  |
|                | 1.4 Latest release and defects report                        | 2  |
|                | 1.5 Licence                                                  | 3  |
|                | 1.6 About the license                                        | 3  |
|                | 1.7 Contributions                                            | 3  |
|                | 1.8 Trademark notice                                         | 3  |
|                | 1.9 Copyright                                                | 3  |
|                | 2 About this document                                        | 6  |
|                | 2.1 Change control                                           | 6  |
|                | 2.1.1 Current status and anticipated changes                 | 6  |
|                | 2.1.2 Change history                                         | 6  |
|                | 2.2 References                                               | 7  |
|                | 2.3 Terms and abbreviations                                  | 8  |
|                | 3 About This Specification                                   | 10 |
|                | 3.1 ELF Class variants                                       | 10 |
|                | 3.1.1 64-bit Pointers, ELF64                                 | 10 |
|                | 3.1.2 32-bit Pointers, ELF32 (Beta)                          | 10 |
|                | 4 Platform standards (Example Only)                          | 11 |
|                | 4.1 Linux Platform ABI (example only)                        | 11 |
|                | 4.1.1 Symbol Versioning                                      | 11 |
|                | 4.1.2 Program Linkage Table (PLT) Sequences and Usage Models | 11 |
| 5 Object Files |                                                              | 12 |
|                | 5.1 Introduction                                             | 12 |
|                | 5.1.1 Registered Vendor Names                                | 12 |
|                | 5.2 ELF Header                                               | 13 |
|                | 5.2.1 ELF Identification                                     | 13 |
|                | 5.3 Sections                                                 | 13 |
|                | 5.3.1 Special Section Indexes                                | 13 |
|                | 5.3.2 Section Types                                          | 13 |
|                | 5.3.3 Section Attribute Flags                                | 14 |
|                | 5.3.4 Special Sections                                       | 14 |
|                | 5.3.5 Section Alignment                                      | 14 |
|                | 5.3.6 Build Attributes                                       | 14 |
|                | 5.4 String Table                                             | 14 |
|                | 5.5 Symbol Table                                             | 14 |
|                | 5.5.1 st_other Values                                        | 15 |
|                | 5.6 Weak Symbols                                             | 16 |
|                | 5.6.1 Weak References                                        | 16 |
|                | 5.6.2 Weak Definitions                                       | 16 |

| 7 Footnotes |                                                     | 42       |
|-------------|-----------------------------------------------------|----------|
|             | 6.5.1 Custom PLTs                                   | 41       |
|             | 6.5 Dynamic Section                                 | 41       |
|             | 6.4 Dynamic Linking                                 | 40       |
|             | 6.3 Program Loading                                 | 40       |
|             | 6.2 Program Property                                | 40       |
|             | 6.1.1 Platform architecture compatibility data      | 40       |
|             | 6.1 Program Header                                  | 40       |
|             | 6 Program Loading and Dynamic Linking               | 40       |
|             | 5.7.17 Idempotency                                  | 39       |
|             | 5.7.16 PAuthABI relocations                         | 39       |
|             | 5.7.15 Unallocated relocations                      | 39       |
|             | 5.7.14 Private and platform-specific relocations    | 39       |
|             | 5.7.13 Dynamic relocations                          | 37       |
|             | 5.7.12 Relocations for PAuth ABI Extension          | 37       |
|             | 5.7.11 Relocations for thread-local storage         | 30       |
|             | 5.7.10 Proxy-generating relocations                 | 29       |
|             | 5.7.9 Relocation optimization                       | 28       |
|             | 5.7.8 Group relocations                             | 28       |
|             | 5.7.7 Call and Jump relocations                     | 27       |
|             | 5.7.6 Static AArch64 relocations                    | 22       |
|             | 5.7.5 Static Data relocations                       | 21       |
|             | 5.7.4 Static miscellaneous relocations              | 21       |
|             | 5.7.2 Addends and PC-bias<br>5.7.3 Relocation types | 19<br>19 |
|             | 5.7.1 Relocation codes                              | 18       |
|             | 5.7 Relocation                                      | 18       |
|             | 5.6.5 Mapping symbols                               | 17       |
|             | 5.6.4 Symbol names                                  | 16       |
|             | 5.6.3 Symbol Types                                  | 16       |
|             |                                                     |          |

# <span id="page-5-0"></span>2 About this document

# <span id="page-5-1"></span>2.1 Change control

# <span id="page-5-2"></span>2.1.1 Current status and anticipated changes

The following support level definitions are used by the Arm ABI specifications:

#### **Release**

Arm considers this specification to have enough implementations, which have received sufficient testing, to verify that it is correct. The details of these criteria are dependent on the scale and complexity of the change over previous versions: small, simple changes might only require one implementation, but more complex changes require multiple independent implementations, which have been rigorously tested for cross-compatibility. Arm anticipates that future changes to this specification will be limited to typographical corrections, clarifications and compatible extensions.

#### **Beta**

Arm considers this specification to be complete, but existing implementations do not meet the requirements for confidence in its release quality. Arm may need to make incompatible changes if issues emerge from its implementation.

### **Alpha**

The content of this specification is a draft, and Arm considers the likelihood of future incompatible changes to be significant.

The ELF32 variant is at "Beta" release quality.

All other content in this document is at the **Release** quality level.

# <span id="page-5-3"></span>2.1.2 Change history

If there is no entry in the change history table for a release, there are no changes to the content of the document for that release.

| Issue    | Date               | Change                                                                                                                                             |
|----------|--------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|
| 00bet3   | 20th December 2011 | Beta release                                                                                                                                       |
| 1.0      | 22nd May 2013      | First public release                                                                                                                               |
| 1.1-beta | 6th November 2013  | ILP32 Beta                                                                                                                                         |
| 2018Q4   | 31st December 2018 | Typographical changes                                                                                                                              |
| 2019Q1   | 29th March 2019    | Add Program Property for BTI and PAC. Update MOV[ZK]<br>related relocations.                                                                       |
| 2019Q2   | 30th June 2019     | Specify STO_AARCH64_VARIANT_PCS. Update<br>R_ <cls>_TLS_DTPREL and R_<cls>_TLS_DTPMOD. Clarify<br/>GNU_PROPERTY_AARCH64_FEATURE_1_AND.</cls></cls> |
| 2019Q4   | 30th January 2020  | Minor layout changes.                                                                                                                              |
| 2020Q2   | 1st July 2020      | Specifiy R_ <cls>_PLT32. Correct minus sign not<br/>rendering in section Group relocations. Adjust table<br/>widths for readability.</cls>         |

| Issue  | Date                   | Change                                                                                                                                                                                                                                                                                |
|--------|------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 2020Q3 | 1st October 2020       | • document released on Github<br>• new Licence: CC-BY-SA-4.0<br>• new sections on Contributions, Trademark notice,<br>and Copyright                                                                                                                                                   |
| 2021Q1 | 12th April 2021        | • Typo fix in definition of GTPREL expression in section<br>Relocations for thread-local storage<br>• Typo fix of EI_OSABI in ELF Identification<br>• Typo fixes -220 -> -2^20 in section Thread-local<br>storage descriptors                                                         |
| 2021Q3 | st November 2021<br>1  | • Reserved relocation codes for PAuthABIELF64                                                                                                                                                                                                                                         |
| 2022Q1 | st April 2022<br>1     | • In<br>Program<br>Property,<br>Soft-deprecate<br>GNU_PROPERTY_AARCH64_FEATURE_1_PAC                                                                                                                                                                                                  |
| 2022Q3 | 20th October 2022      | • In Dynamic relocations, include the ABS64 and<br>ABS32 relocations in Dynamic relocations.<br>• In Relocation optimization, ADRP + LDR GOT<br>relaxation symbol should not be absolute.<br>• In Program Loading and Dynamic Linking, document<br>new PT_AARCH64_MEMTAG_MTE segment. |
| 2023Q3 | th October 2023<br>6   | • In<br>Symbol<br>Table,<br>Expand<br>STO_AARCH64_VARIANT_PCS documentation<br>• In Program Loading, Relax BTI PLT requirement                                                                                                                                                        |
| 2024Q3 | th September 2024<br>5 | • In<br>Program<br>Property,<br>Program<br>Loading,<br>and<br>Dynamic Linking, move description to SYSVABI64.<br>• Move relocation codes from PAUTHABIELF64 into<br>reserved space.<br>• Clarify use of addends in MOVZ, MOVK,<br>and ADRP                                            |

# <span id="page-6-0"></span>2.2 References

This document refers to, or is referred to by, the following documents.

| Ref     | External reference or URL | Title                                             |
|---------|---------------------------|---------------------------------------------------|
| AAELF64 | Source for this document  | ELF for the Arm 64-bit Architecture<br>(AArch64). |

| Ref               | External reference or URL                                                                            | Title                                                                          |
|-------------------|------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| AAPCS64           | IHI 0055                                                                                             | Procedure Call Standard for the Arm<br>64-bit Architecture                     |
| Addenda32         | IHI 0045                                                                                             | Addenda to, and Errata in, the ABI for<br>the Arm Architecture                 |
| PAuthABIELF<br>64 | pauthabielf64                                                                                        | PAuth Extension to ELF for the Arm<br>64-bit Architecture                      |
| LSB               | http://www.linuxbase.org/                                                                            | Linux Standards Base                                                           |
| SCO-ELF           | http://www.sco.com/developers/gabi/                                                                  | System V Application Binary Interface<br>– DRAFT                               |
| LINUX_ABI         | https://github.com/hjl-tools/linux-abi/wiki                                                          | Linux Extensions to gABI                                                       |
| SYM-VER           | http://people.redhat.com/drepper/symbol-ver<br>sioning                                               | GNU Symbol Versioning                                                          |
| TLSDESC           | http://www.fsfla.org/~lxoliva/writeups/TLS/pa<br>per-lk2006.pdf                                      | TLS Descriptors for Arm. Original<br>proposal document                         |
| MTEEXTENSI<br>ONS | https://www.kernel.org/doc/html/latest/arm6<br>4/memory-tagging-extension.html#core-dum<br>p-support | Linux Kernel MTE core dump format                                              |
| SYSVABI64         | sysvabi64                                                                                            | System V Application Binary Interface<br>(ABI) for the Arm 64-bit Architecture |

# <span id="page-7-0"></span>2.3 Terms and abbreviations

The ABI for the Arm 64-bit Architecture uses the following terms and abbreviations:

#### **A32**

The instruction set named Arm in the Armv7 architecture; A32 uses 32-bit fixed-length instructions.

### **A64**

The instruction set available when in AArch64 state.

#### **AAPCS64**

Procedure Call Standard for the Arm 64-bit Architecture (AArch64)

### **AArch32**

The 32-bit general-purpose register width state of the Armv8 architecture, broadly compatible with the Armv7-A architecture.

#### **AArch64**

The 64-bit general-purpose register width state of the Armv8 architecture.

#### **ABI**

Application Binary Interface:

- 1. The specifications to which an executable must conform in order to execute in a specific execution environment. For example, the Linux ABI for the Arm Architecture.
- 2. A particular aspect of the specifications to which independently produced relocatable files must conform in order to be statically linkable and executable. For exam[ple, the Addenda32,](https://github.com/ARM-software/abi-aa/releases) [AAPCS64,](https://github.com/ARM-software/abi-aa/releases) ...

#### **Arm-based**

... based on the Arm architecture ...

#### **ELF32**

An ELF object file with a class of ELFCLASS32

### **ELF64**

An ELF object file with a class of ELFCLASS64

#### **ILP32**

SysV-like data model where int, long int and pointer are 32-bit

#### **LP64**

SysV-like data model where int is 32-bit, but long int and pointer are 64-bit.

#### **Q-o-I**

Quality of Implementation – a quality, behavior, functionality, or mechanism not required by this standard, but which might be provided by systems conforming to it. Q-o-I is often used to describe the toolchain-specific means by which a standard requirement is met.

#### **T32**

The instruction set named Thumb in the Armv7 architecture; T32 uses 16-bit and 32-bit instructions. Other terms may be defined when first used.

# <span id="page-9-0"></span>3 About This Specification

This specification provides the processor-specific definitions required by ELF [[SCO-ELF\]](http://www.sco.com/developers/gabi/) for AArch64-based systems.

The ELF specification is part of the larger Unix System V (SysV) ABI specification where it for[ms Object](#page-11-3) [Files](#page-11-3) and [Program Loading and Dynamic Linking](#page-39-7). However, the ELF specification can be used in isolation as a generic object and executable format. [Platform standards \(Example Only\)](#page-10-4) covers ELF related matters that are platform specific.

[Object Files](#page-11-3) and [Program Loading and Dynamic Linking](#page-39-7) are structured to correspond to chapters 4 and 5 of the ELF specification. Specifically:

- [Object Files](#page-11-3) covers object files and relocations
- [Program Loading and Dynamic Linking](#page-39-7) covers program loading and dynamic linking.

# <span id="page-9-1"></span>3.1 ELF Class variants

Two different pointer sizes are supported by this specification, which result in two very similar but different ELF definitions.

### <span id="page-9-2"></span>3.1.1 64-bit Pointers, ELF64

- Code and data using 64-bit pointers are contained in an ELF object file with a class of **ELFCLASS64**.
- Referred to as **ELF64** in this specification.
- Pointer-size is **64 bits**.
- Suitable for use by the LP64 variant of [[AAPCS64\]](https://github.com/ARM-software/abi-aa/releases)

### <span id="page-9-3"></span>3.1.2 32-bit Pointers, ELF32 **(Beta)**

- Code and data using 32-bit pointers is contained in an ELF object file with a class of **ELFCLASS32**.
- Referred to as **ELF32** in this specification.
- Pointer-size is **32 bits**.
- Suitable for use by the ILP32 variant of [[AAPCS64](https://github.com/ARM-software/abi-aa/releases)]

### Note

Interlinking is not supported between the ELF32 and ELF64 variants.

# <span id="page-10-4"></span><span id="page-10-0"></span>4 Platform standards (Example Only)

We expect that each operating system that adopts components of this ABI specification will specify additional requirements and constraints that must be met by application code in binary form and the code-generation tools that generate such code.

As an example of the kind of issue that must be addressed, [Linux Platform ABI \(example only\)](#page-10-5) lists some of the issues addressed by the *Linux Standard Base* [[LSB\]](http://www.linuxbase.org/) specifications.

# <span id="page-10-5"></span><span id="page-10-1"></span>4.1 Linux Platform ABI (example only)

### <span id="page-10-2"></span>4.1.1 Symbol Versioning

The Linux ABI uses the GNU-extended Solaris symbol versioning mechanism [[SYM-VER\]](http://www.akkadia.org/drepper/symbol-versioning).

Concrete data structure descriptions can be found in /usr/include/sys/link.h (Solaris), /usr/include/elf.h (Linux), in the *Linux Standard Base specifications* [\[LSB\]](http://www.linuxbase.org/), and in Drepper's paper [\[SYM-VER\]](http://www.akkadia.org/drepper/symbol-versioning).

A binary file intended to be specific to Linux shall set the EI\_OSABI field to the value required by Linux [\[LSB](http://www.linuxbase.org/)].

# <span id="page-10-3"></span>4.1.2 Program Linkage Table (PLT) Sequences and Usage Models

### 4.1.2.1 Symbols for which a PLT entry must be generated

A PLT entry implements a long-branch to a destination outside of this executable file. In general, the static linker knows only the name of the destination. It does not know its address. Such a location is called an imported location or imported symbol.

SysV-based Dynamic Shared Objects (DSOs) (e.g. for Linux) also require functions exported from an executable file to have PLT entries. In effect, exported functions are treated as if they were imported, so that their definitions can be overridden (pre-empted) at dynamic link time.

A linker must generate a PLT entry for each candidate symbol cited by a relocation directive that relocates an AArch64 B/BL-class instruction [\(Call and Jump relocations\)](#page-26-1). For a Linux/SysV DSO, each STB\_GLOBAL symbol with STV\_DEFAULT visibility is a candidate.

### 4.1.2.2 Overview of PLT entry code generation

A PLT entry must be able to branch any distance. This is typically achieved by loading the destination address from the corresponding Global Object Table (GOT) entry.

On-demand dynamic linking constrains the code sequences that can be generated for a PLT entry. Specifically, there is a requirement from the dynamic linker for certain registers to contain certain values. Typically these are:

- The address or index of the not-yet-linked PLT entry.
- The return address of the call to the PLT entry.

The register interface to the dynamic linker is specified by the host operating system.

# <span id="page-11-3"></span><span id="page-11-0"></span>5 Object Files

# <span id="page-11-1"></span>5.1 Introduction

# <span id="page-11-4"></span><span id="page-11-2"></span>5.1.1 Registered Vendor Names

Various symbols and names may require a vendor-specific name to avoid the potential for name-space conflicts. The list of currently registered vendors and their preferred short-hand name is given in the below table. Tools developers not listed are requested to co-ordinate with Arm to avoid the potential for conflicts.

#### Registered Vendors

| Name               | Vendor                                                                                                         |  |  |  |
|--------------------|----------------------------------------------------------------------------------------------------------------|--|--|--|
| aeabi              | Reserved to the ABI for the Arm Architecture (EABI pseudo-vendor)                                              |  |  |  |
| AnonXyz<br>anonXyz | Reserved to private experiments by the Xyz vendor. Guaranteed not to clash with any<br>registered vendor name. |  |  |  |
| ARM                | Arm Ltd (Note: the company, not the processor).                                                                |  |  |  |
| cxa                | C++ ABI pseudo-vendor                                                                                          |  |  |  |
| dig                | Dignus, LLC.                                                                                                   |  |  |  |
| FSL                | Freescale Semiconductor Inc.                                                                                   |  |  |  |
| GHS                | Green Hills Systems                                                                                            |  |  |  |
| gnu                | GNU compilers and tools (Free Software Foundation)                                                             |  |  |  |
| iar                | IAR Systems                                                                                                    |  |  |  |
| intel              | Intel Corporation                                                                                              |  |  |  |
| ixs                | Intel Xscale                                                                                                   |  |  |  |
| llvm               | The LLVM/Clang projects                                                                                        |  |  |  |
| mchp               | Microchip Technology Inc.                                                                                      |  |  |  |
| PSI                | PalmSource Inc.                                                                                                |  |  |  |
| RAL                | Rowley Associates Ltd                                                                                          |  |  |  |
| somn               | SOMNIUM Technologies Limited.                                                                                  |  |  |  |
| TASKING            | Altium Ltd.                                                                                                    |  |  |  |
| TI                 | TI Inc.                                                                                                        |  |  |  |
| tls                | Reserved for use in thread-local storage routines.                                                             |  |  |  |
| WRS                | Wind River Systems.                                                                                            |  |  |  |

To register a vendor prefix with Arm, please E-mail your request to arm.eabi at arm.com.

# <span id="page-12-0"></span>5.2 ELF Header

The ELF header provides a number of fields that assist in interpretation of the file. Most of these are specified in the base standard. The following fields have Arm-specific meanings.

e\_machine

An object file conforming to this specification must have the value EM\_AARCH64 (183, 0xB7).

e\_entry

The base ELF specification requires this field to be zero if an application does not have an entry point. Nonetheless, some applications may require an entry point of zero (for example, via a reset vector).

A platform standard may specify that an executable file always has an entry point, in which case e\_entry specifies that entry point, even if zero.

e\_flags

There are no processor-specific flags so this field shall contain zero.

### <span id="page-12-5"></span><span id="page-12-1"></span>5.2.1 ELF Identification

The 16-byte ELF identification (e\_ident) provides information on how to interpret the file itself. The following values shall be used on Arm systems

EI\_CLASS

For object files (executable, shared and relocatable) the **EI\_CLASS** shall be:

- ELFCLASS64 for an ELF64 object file.
- ELFCLASS32 for an ELF32 object file **(Beta)**.

EI\_DATA

This field may be either ELFDATA2LSB or ELFDATA2MSB. The choice will be governed by the default data order in the execution environment.

EI\_OSABI

This field shall be zero unless the file uses objects that have flags which have OS-specific meanings (for example, it makes use of a section index in the range SHN\_LOOS through SHN\_HIOS).

# <span id="page-12-2"></span>5.3 Sections

### <span id="page-12-3"></span>5.3.1 Special Section Indexes

No processor-specific special section indexes are defined. All processor-specific values are reserved to future revisions of this specification.

### <span id="page-12-4"></span>5.3.2 Section Types

The defined processor-specific section types are listed in the below table. All other processor-specific values are reserved to future revisions of this specification.

#### Processor specific section types

| Name                   | Value      | Comment                                           |  |
|------------------------|------------|---------------------------------------------------|--|
| SHT_AARCH64_ATTRIBUTES | 0x70000003 | Reserved for Object file compatibility attributes |  |

## <span id="page-13-0"></span>5.3.3 Section Attribute Flags

There are no processor-specific section attribute flags defined. All processor-specific values are reserved to future revisions of this specification.

### 5.3.3.1 Merging of objects in sections with SHF\_MERGE

In a section with the SHF\_MERGE flag set, duplicate used objects may be merged and unused objects may be removed. An object is used if:

- A relocation directive addresses the object via the section symbol with a suitable addend to point to the object.
- A relocation directive addresses a symbol within the section. The used object is the one addressed by the symbol irrespective of the addend used.

### <span id="page-13-1"></span>5.3.4 Special Sections

The below table lists the special sections defined by this ABI.

### AArch64 special sections

| Name               | Type                   | Attributes |
|--------------------|------------------------|------------|
| .ARM.attributes    | SHT_AARCH64_ATTRIBUTES | none       |
| .note.gnu.property | SHT_NOTE               | SHF_ALLOC  |

<sup>.</sup>ARM.attributes names a section that contains build attributes. See [Build Attributes.](#page-13-7)

Additional special sections may be required by some platforms standards.

### <span id="page-13-2"></span>5.3.5 Section Alignment

There is no minimum alignment required for a section. Sections containing code must be at least 4-byte aligned. Platform standards may set a limit on the maximum alignment that they can guarantee (normally the minimum page size supported by the platform).

### <span id="page-13-7"></span><span id="page-13-3"></span>5.3.6 Build Attributes

Build attributes are encoded in a section of type SHT\_AARCH64\_ATTRIBUTES, and name .ARM.attributes.

Build attributes are unnecessary when a platform ABI operating system is fully specified. At this time no public build attributes have been defined for AArch64, however, software development tools are free to use attributes privately. For an introduction to AArch32 build attributes see [[Addenda32\]](https://github.com/ARM-software/abi-aa/releases).

# <span id="page-13-4"></span>5.4 String Table

There are no processor-specific extensions to the string table.

# <span id="page-13-6"></span><span id="page-13-5"></span>5.5 Symbol Table

There are no processor-specific symbol types or symbol bindings. All processor-specific values are reserved to future revisions of this specification.

<sup>.</sup>note.gnu.property names a section that holds a program property note. S[ee \[LINUX\\_ABI](https://github.com/hjl-tools/linux-abi/wiki)] for more information.

# <span id="page-14-0"></span>5.5.1 st\_other Values

The st\_other member of a symbol table entry specifies the symbol's visibility in the lowest 2 bits. The top 6 bits are unused in the generic ELF A[BI \[SCO-ELF](http://www.sco.com/developers/gabi/)], and while there are no values reserved for processor-specific semantics, many other architectures have used these bits.

The defined processor-specific st\_other flag values are listed below:

### Processor specific st\_other flags

| Name                    | Mask | Comment                                                                                                                              |
|-------------------------|------|--------------------------------------------------------------------------------------------------------------------------------------|
| STO_AARCH64_VARIANT_PCS | 0x80 | The function associated with the symbol may follow a variant<br>procedure call standard with different register usage<br>convention. |

A symbol table entry that is marked with the STO\_AARCH64\_VARIANT\_PCS flag set in its st\_other field may be associated with a function that follows a variant procedure call standard under which:

- the caller and callee exchange information in registers that are not set aside for that purpose in the base procedure call standard [[AAPCS64\]](https://github.com/ARM-software/abi-aa/releases); or
- the processor is guaranteed or allowed to be in a certain state on entry or return from the function, beyond or in conflict with the state guaranteed or allowed by the base procedure call standard.

#### For example:

- The function might take arguments in registers that are not normally argument registers.
- The function might return values in registers that are not normally return value registers.
- The function might guarantee that extra register state is preserved by the call.
- PSTATE on entry to the function might be different from normal.
- PSTATE on return from the function might be different from normal.

The following types of function must be marked with STO\_AARCH64\_VARIANT\_PCS, although the list is not intended to be exhaustive:

- vector PCS functions [[VFABI64\]](https://github.com/ARM-software/abi-aa/releases)
- functions that take arguments in SVE registers or return values in SVE registers
- streaming and streaming-compatible functions [[AAPCS64](https://github.com/ARM-software/abi-aa/releases)]
- shared-ZA functions [[AAPCS64](https://github.com/ARM-software/abi-aa/releases)]

The rules in the [Call and Jump relocations](#page-26-1) section still apply to variant PCS functions.

If a subroutine is called via a symbol reference that is marked with STO\_AARCH64\_VARIANT\_PCS, then code that runs between the calling routine and the called subroutine must preserve the contents of all registers except for IP0, IP1, and the condition code fl[ags \[AAPCS64\]](https://github.com/ARM-software/abi-aa/releases). It must also preserve the processor execution mode, such as PSTATE.SM and PSTATE.ZA. It is not possible to provide a definitive list of which state must be preserved, since the intention is to allow it to grow as the architecture evolves.

Static linkers must preserve the marking and propagate it to the dynamic symbol table if any reference or definition of the symbol is marked with STO\_AARCH64\_VARIANT\_PCS, and add a DT\_AARCH64\_VARIANT\_PCS dynamic tag if required by the [Dynamic Section](#page-40-2) section.

### Note

In particular, when a call is made via the PLT entry of a symbol marked with STO\_AARCH64\_VARIANT\_PCS, a dynamic linker cannot assume that the call follows the register usage convention of the base procedure call standard.

# <span id="page-15-0"></span>5.6 Weak Symbols

There are two forms of weak symbol:

- A weak reference This is denoted by:
  - st\_shndx=SHN\_UNDEF, ELF64\_ST\_BIND()=STB\_WEAK.
  - st\_shndx=SHN\_UNDEF, ELF32\_ST\_BIND()=STB\_WEAK **(Beta)**.
- A weak definition This is denoted by:
  - st\_shndx!=SHN\_UNDEF, ELF64\_ST\_BIND()=STB\_WEAK.
  - st\_shndx!=SHN\_UNDEF, ELF32\_ST\_BIND()=STB\_WEAK **(Beta)**.

### <span id="page-15-1"></span>5.6.1 Weak References

Libraries are not searched to resolve weak references. It is not an error for a weak reference to remain unsatisfied.

During linking, the symbol value of an undefined weak reference is:

- Zero if the relocation type is absolute
- The address of the place if the relocation type is pc-relative.

See [Relocation](#page-17-2) for further details.

### <span id="page-15-2"></span>5.6.2 Weak Definitions

A weak definition does not change the rules by which object files are selected from libraries. However, if a link set contains both a weak definition and a non-weak definition, the non-weak definition will always be used.

# <span id="page-15-3"></span>5.6.3 Symbol Types

All code symbols exported from an object file (symbols with binding STB\_GLOBAL) shall have type STT\_FUNC. All extern data objects shall have type STT\_OBJECT. No STB\_GLOBAL data symbol shall have type STT\_FUNC. The type of an undefined symbol shall be STT\_NOTYPE or the type of its expected definition.

The type of any other symbol defined in an executable section can be STT\_NOTYPE. A linker is only required to provide long-branch and PLT support for symbols of type STT\_FUNC.

# <span id="page-15-4"></span>5.6.4 Symbol names

A symbol that names a C or assembly language entity should have the name of that entity. For example, a C function called calculate generates a symbol called calculate (not \_calculate).

Symbol names are case sensitive and are matched exactly by linkers.

Any symbol with binding STB\_LOCAL may be removed from an object and replaced with an offset from another symbol in the same section under the following conditions:

- The original symbol and replacement symbol are not of type STT\_FUNC, or both symbols are of type STT\_FUNC.
- All relocations referring to the symbol can accommodate the adjustment in the addend field (it is permitted to convert a REL type relocation to a RELA type relocation).
- The symbol is not described by the debug information.

- The symbol is not a mapping symbol ([Mapping symbols\)](#page-16-1).
- The resulting object, or image, is not required to preserve accurate symbol information to permit de-compilation or other post-linking optimization techniques.
- If the symbol labels an object in a section with the SHF\_MERGE flag set, the relocation using symbol may be changed to use the section symbol only if the initial addend of the relocation is zero.

No tool is required to perform the above transformations; an object consumer must be prepared to do this itself if it might find the additional symbols confusing.

#### Note

Multiple conventions exist for the names of compiler temporary symbols (for example, ARMCC uses Lxxx.yyy, while GNU tools use .Lxxx).

### 5.6.4.1 Reserved symbol names

The following symbols are reserved to this and future revisions of this specification:

- Local symbols (STB\_LOCAL) beginning with '\$'
- Symbols matching the pattern {non-empty-prefix}\$\${non-empty-suffix}.
- Global symbols (STB\_GLOBAL, STB\_WEAK) beginning with '\_\_aeabi\_' (double '\_' at start).

#### Note

Global symbols beginning with '\_\_vendor\_' (double '\_' at start), where vendor is listed [in Registered](#page-11-4) [Vendor Names](#page-11-4) are reserved to the named vendor for the purpose of providing vendor-specific toolchain support functions.

### <span id="page-16-1"></span><span id="page-16-0"></span>5.6.5 Mapping symbols

A section of an ELF file can contain a mixture of A64 code and data. There are inline transitions between code and data at literal pool boundaries.

Linkers, file decoders and other tools need to map binaries correctly. To support this, a number of symbols, termed mapping symbols appear in the symbol table to label the start of each sequence of bytes of the appropriate class. All mapping symbols have type STT\_NOTYPE and binding STB\_LOCAL. The st\_size field is unused and must be zero.

The mapping symbols are defined in the [Mapping symbols table](#page-17-3). It is an error for a relocation to reference a mapping symbol. Two forms of mapping symbol are supported:

- A short form that uses a dollar character and a single letter denoting the class. This form can be used when an object producer creates mapping symbols automatically. Its use minimizes string table size.
- A longer form in which the short form is extended with a period and then any sequence of characters that are legal for a symbol. This form can be used when assembler files have to be annotated manually and the assembler does not support multiple definitions of symbols.

Mapping symbols defined in a section (relocatable view) or segment (executable view) define a sequence of half- open intervals that cover the address range of the section or segment. Each interval starts at the address defined by the mapping symbol, and continues up to, but not including, the address defined by the next (in address order) mapping symbol or the end of the section or segment. A section that contains instructions must have a mapping symbol defined at the beginning of the section. If a section contains only data no mapping symbol is required. A platform ABI should specify whether or not mapping symbols are present in the executable view; they will never be present in a stripped executable file.

#### <span id="page-17-3"></span>Mapping symbols

| Name                    | Meaning                                                         |
|-------------------------|-----------------------------------------------------------------|
| \$x<br>\$x. <any></any> | Start of a sequence of A64 instructions                         |
| \$d<br>\$d. <any></any> | Start of a sequence of data items (for example, a literal pool) |

# <span id="page-17-2"></span><span id="page-17-0"></span>5.7 Relocation

Relocation information is used by linkers to bind symbols to addresses that could not be determined when the binary file was generated. Relocations are classified as *Static* or *Dynamic*.

- A *static relocation* relocates a place in an ELF relocatable file (e\_type = ET\_REL); a static linker processes it.
- A *dynamic relocation* is designed to relocate a place in an ELF executable file or dynamic shared object (e\_type = ET\_EXEC, ET\_DYN) and to be handled by a dynamic linker, program loader, or other post-linking tool (dynamic linker henceforth).
- A dynamic linker need only process dynamic relocations; a static linker must handle any defined relocation.
- Dynamic relocations are designed to be processed quickly.
  - There are a small number of dynamic relocations whose codes are contiguous.
  - Dynamic relocations relocate simple places and do not need complex field extraction or insertion.
- A static linker either:
  - Fully resolves a relocation directive.
  - Or, generates a dynamic relocation from it for processing by a dynamic linker.
- A well-formed executable file or dynamic shared object has no static relocations after static linking.

### <span id="page-17-1"></span>5.7.1 Relocation codes

The relocation codes for AArch64 are divided into four categories:

- Mandatory relocations that must be supported by all static linkers.
- Platform-specific relocations required by specific platform ABIs.
- Private relocations that are guaranteed never to be allocated in future revisions of this specification, but which must never be used in portable object files.
- Unallocated relocations that are reserved for use in future revisions of this specification.

## <span id="page-18-2"></span><span id="page-18-0"></span>5.7.2 Addends and PC-bias

A binary file may use REL or RELA relocations or a mixture of the two (but multiple relocations of the same place must use only one type).

The initial addend for a REL-type relocation is formed according to the following rules.

- If the relocation relocates data ([Static Data relocations](#page-20-2)) the initial value in the place is sign-extended to 64 bits.
- If the relocation relocates an instruction the immediate field of the instruction is extracted, scaled as required by the instruction field encoding, and sign-extended to 64 bits.

A RELA format relocation must be used if the initial addend cannot be encoded in the place.

There is no PC bias to accommodate in the relocation of a place containing an instruction that formulates a PC- relative address. The program counter reflects the address of the currently executing instruction.

There are two special cases for forming the initial addend of REL-type relocations where the immediate field cannot normally hold small signed integers:

- For relocations processing MOVZ and MOVK instructions (including the "MOV (wide immediate)" alias), the initial addend is formed by interpreting the 16-bit literal field of the instruction as a 16-bit signed value in the range -32768 <= A < 32768. The interpretation is the same whether or not the instruction applies a left shift to its immediate: the addend is never treated as shifted.
- For relocations processing the ADRP instruction, the initial addend is similarly formed by interpreting the literal field of the instruction as a 21-bit signed integer, in the range -1048576 <= A < 1048576. The ADRP instruction's implicit left shift of 12 bits is not applied.

These special cases permit a sequence of instructions to each add the same small constant to a symbol's value, and extract separate ranges of bits from the sum, so that the instruction sequence as a whole consistently loads the full result of the addition.

In the case of a sequence using ADRP followed by a 12-bit ADD to set up the low bits of the offset, you can express an offset up to 1048576 in either direction, by writing the full offset in the ADRP's immediate field, and repeating its low 12 bits in the ADD's immediate field. A linker resolving the R\_AARCH64\_ADD\_ABS\_LO12\_NC relocation on the ADD will not compute the correct overall 64-bit value, but the error will only be in the higher bits, which are discarded by that relocation.

### <span id="page-18-3"></span><span id="page-18-1"></span>5.7.3 Relocation types

Tables in the following sections list the relocation codes for AArch64 and record the following.

- The relocation code which is stored in the ELF64\_R\_TYPE or ELF32\_R\_TYPE component of the r\_info field.
- The preferred mnemonic name for the relocation. This has no significance in a binary file.
- The relocation operation required. This field describes how a symbol and addend are processed by a linker. It does not describe how an initial addend value is extracted from a p[lace \(Addends and](#page-18-2) [PC-bias\)](#page-18-2) or how the resulting relocated value is inserted or encoded into a place.
- A comment describing the kind of place that can be relocated, the part of the result value inserted into the place, and whether or not field overflow should be checked.

### 5.7.3.1 Relocation names and class

A mnemonic name class is used to distinguish between ELF64 and ELF32 relocation names.

- ELF64 relocations have <CLS> = AARCH64, e.g. R\_AARCH64\_ABS32
- ELF32 relocations have <CLS> = AARCH64\_P32, where P32 denotes the pointer size, e.g. R\_AARCH64\_P32\_ABS32 **(Beta)**

Within this document <CLS> is not expanded in instances where only a single relocation name exists.

### 5.7.3.2 Relocation codes disambiguation

References to relocation codes are disambiguated in the following way:

- ELF64 relocation codes are bounded by parentheses: ( ).
- ELF32 relocation codes are bounded by brackets: [ ].

Static relocation codes for ELF64 object files begin at (257); dynamic ones at (1024). Both (0) and (256) should be accepted as values of R\_AARCH64\_NONE, the null relocation.

Static relocation codes for ELF32 object files begin at [1]; dynamic ones at [180].

All unallocated type codes are reserved for future allocation.

### 5.7.3.3 Relocation operations

The following nomenclature is used in the descriptions of relocation operations:

- S (when used on its own) is the address of the symbol.
- A is the addend for the relocation.
- P is the address of the place being relocated (derived from r\_offset).
- X is the result of a relocation operation, before any masking or bit-selection operation is applied
- Page(expr) is the page address of the expression expr, defined as (expr & ~0xFFF). (This applies even if the machine page size supported by the platform has a different value.)
- GOT is the address of the Global Offset Table, the table of code and data addresses to be resolved at dynamic link time. The GOT and each entry in it must be, 64-bit aligned for ELF64 or 32-bit aligned for ELF32.
- GDAT(S+A) represents a pointer-sized entry in the GOT for address S+A. The entry will be relocated at run time with relocation R\_<CLS>\_GLOB\_DAT(S+A).
- G(expr) is the address of the GOT entry for the expression expr.
- Delta(S) if S is a normal symbol, resolves to the difference between the static link address of S and the execution address of S. If S is the null symbol (ELF symbol index 0), resolves to the difference between the static link address of P and the execution address of P.
- Indirect(expr) represents the result of calling expr as a function. The result is the return value from the function that is returned in r0. The arguments passed to the function are defined by the platform ABI.
- [msb:lsb] is a bit-mask operation representing the selection of bits in a value. The bits selected range from lsb up to msb inclusive. For example, 'bits [3:0]' represents the bits under the mask 0x0000000F. When range checking is applied to a value, it is applied before the masking operation is performed.

The value written into a target field is always reduced to fit the field. It is Q-o-I whether a linker generates a diagnostic when a relocated value overflows its target field.

Relocation types whose names end with "\_NC" are non-checking relocation types. These must not generate diagnostics in case of field overflow. Usually, a non-checking type relocates an instruction that computes one of the less significant parts of a single value computed by a group of instruction[s \(Group](#page-27-2) [relocations](#page-27-2)). Only the instruction computing the most significant part of the value can be checked for field overflow because, in general, a relocated value will overflow the fields of instructions computing the less significant parts. Some non-checking relocations may, however, be expected to check for correct alignment of the result; the notes explain when this is permitted. In ELF32 relocations an overflow check of -231 <= X < 231 or 0 <= X < 231 is equivalent to no check (i.e. 'None').

In ELF32 **(Beta)** relocations additional care must be taken when relocating an ADRP instruction which effectively uses a signed 33-bit PC-relative offset to generate a 32-bit address. The following relocations apply to ADRP:

```
R_<CLS>_ADR_PREL_PG_HI21,
R_<CLS>_ADR_GOT_PAGE,
R_<CLS>_TLSGD_ADR_PAGE21,
R_<CLS>_TLSLD_ADR_PAGE21,
R_<CLS>_TLSIE_ADR_GOTTPREL_PAGE21,
R_<CLS>_TLSDESC_ADR_PAGE21
```

Relocations using the GDAT(S) operation must have a zero addend. Previous versions of this document included the addend A in GDAT(S + A) resulting in a GOT entry for S + A. With a zero addend GDAT(S + 0) is equivalent to GDAT(S) and GDAT(S) + 0.

### <span id="page-20-0"></span>5.7.4 Static miscellaneous relocations

R\_<CLS>\_NONE (null relocation code) records that the section containing the place to be relocated depends on the section defining the symbol mentioned in the relocation directive in a way otherwise invisible to a static linker. The effect is to prevent removal of sections that might otherwise appear to be unused.

### Null relocation codes

| ELF64 Code | ELF32 Code | Name                | Operation | Comment                       |
|------------|------------|---------------------|-----------|-------------------------------|
| 0          | 0          | R_ <cls>_NONE</cls> | None      |                               |
| 256        | -          | withdrawn           | None      | Treat as R_ <cls>_NONE.</cls> |

# <span id="page-20-2"></span><span id="page-20-1"></span>5.7.5 Static Data relocations

See also table [GOT-relative data relocations.](#page-25-0)

### Data relocations

| ELF64 Code | ELF32 Code | Name                  | Operation | Comment                                                                          |
|------------|------------|-----------------------|-----------|----------------------------------------------------------------------------------|
| 257        | -          | R_ <cls>_ABS64</cls>  | S + A     | Write bits [63:0] of X at<br>byte-aligned place P. No overflow<br>check.         |
| 258        | 1          | R_ <cls>_ABS32</cls>  | S + A     | Write bits [31:0] of X at<br>byte-aligned place P. Check that<br>-231 <= X < 232 |
| 259        | 2          | R_ <cls>_ABS16</cls>  | S + A     | Write bits [15:0] of X at<br>byte-aligned place P. Check that<br>-215 <= X < 216 |
| 260        | -          | R_ <cls>_PREL64</cls> | S + A - P | Write bits [63:0] of X at<br>byte-aligned place P. No overflow<br>check.         |

| ELF64 Code | ELF32 Code | Name                  | Operation | Comment                                                                                                            |
|------------|------------|-----------------------|-----------|--------------------------------------------------------------------------------------------------------------------|
| 261        | 3          | R_ <cls>_PREL32</cls> | S + A - P | Write bits [31:0] of X at<br>byte-aligned place P. Check that<br>-231 <= X < 232                                   |
| 262        | 4          | R_ <cls>_PREL16</cls> | S + A - P | Write bits [15:0] of X at<br>byte-aligned place P. Check that<br>-215 <= X < 216                                   |
| 314        | 29         | R_ <cls>_PLT32</cls>  | S + A - P | Write bits [31:0] of X at<br>byte-aligned place P. Check that<br>-231 <= X < 231 see call and<br>jump relocations. |

These overflow ranges permit either signed or unsigned narrow values to be created from the intermediate result viewed as a 64-bit signed integer. If the place is intended to hold a narrow signed value and INTn\_MAX < X <= UINTn\_MAX, no overflow will be detected but the positive result will be interpreted as a negative value.

## <span id="page-21-0"></span>5.7.6 Static AArch64 relocations

The following tables record single instruction relocations and relocations that allow a group or sequence of instructions to compute a single relocated value.

#### <span id="page-21-1"></span>Group relocations to create a 16-, 32-, 48-, or 64-bit unsigned data value or address inline

| ELF64<br>Code | ELF32<br>Code | Name                           | Operation | Comment                                                                             |
|---------------|---------------|--------------------------------|-----------|-------------------------------------------------------------------------------------|
| 263           | 5             | R_ <cls>_MOVW_UABS_G0</cls>    | S + A     | Set a MOV[KZ] immediate field to<br>bits [15:0] of X; check that 0 <=<br>X < 216    |
| 264           | 6             | R_ <cls>_MOVW_UABS_G0_NC</cls> | S + A     | Set a MOV[KZ] immediate field to<br>bits [15:0] of X. No overflow<br>check          |
| 265           | 7             | R_ <cls>_MOVW_UABS_G1</cls>    | S + A     | Set a MOV[KZ] immediate field to<br>bits [31:16] of X; check that 0 <=<br>X < 232   |
| 266           | -             | R_ <cls>_MOVW_UABS_G1_NC</cls> | S + A     | Set a MOV[KZ] immediate field to<br>bits [31:16] of X. No overflow<br>check         |
| 267           | -             | R_ <cls>_MOVW_UABS_G2</cls>    | S + A     | Set a MOV[KZ] immediate field to<br>bits [47:32] of X; check that 0 <=<br>X < 248   |
| 268           | -             | R_ <cls>_MOVW_UABS_G2_NC</cls> | S + A     | Set a MOV[KZ] immediate field to<br>bits [47:32] of X. No overflow<br>check         |
| 269           | -             | R_ <cls>_MOVW_UABS_G3</cls>    | S + A     | Set a MOV[KZ] immediate field to<br>bits [63:48] of X (no overflow<br>check needed) |

<span id="page-21-2"></span>Group relocations to create a 16, 32, 48, or 64 bit signed data or offset value inline

| ELF64<br>Code | ELF32<br>Code | Name                        | Operation | Comment                                                                                                                |
|---------------|---------------|-----------------------------|-----------|------------------------------------------------------------------------------------------------------------------------|
| 270           | 8             | R_ <cls>_MOVW_SABS_G0</cls> | S + A     | Set a MOV[NZ] immediate field using bits [15:0] of X (see notes below); check -2 <sup>16</sup> <= X < 2 <sup>16</sup>  |
| 271           | -             | R_ <cls>_MOVW_SABS_G1</cls> | S + A     | Set a MOV[NZ] immediate field using bits [31:16] of X (see notes below); check -2 <sup>32</sup> <= X < 2 <sup>32</sup> |
| 272           | -             | R_ <cls>_MOVW_SABS_G2</cls> | S + A     | Set a MOV[NZ] immediate field using bits [47:32] of X (see notes below); check $-2^{48} <= X < 2^{48}$                 |

These checking forms relocate  ${\tt MOVN}$  or  ${\tt MOVZ}.$ 

X >= 0: Set the instruction to MOVZ and its immediate field to the selected bits of X.

X < 0: Set the instruction to MOVN and its immediate field to NOT (selected bits of X).

### Relocations to generate 19, 21 and 33 bit PC-relative addresses

| ELF64<br>Code | ELF32<br>Code | Name                                    | Operation             | Comment                                                                                                                             |
|---------------|---------------|-----------------------------------------|-----------------------|-------------------------------------------------------------------------------------------------------------------------------------|
| 273           | 9             | R_ <cls>_ LD_PREL_LO19</cls>            | S + A - P             | Set a load-literal immediate value to bits [20:2] of X; check that $-2^{20} \le X \le 2^{20}$                                       |
| 274           | 10            | R_ <cls>_ ADR_PREL_LO21</cls>           | S + A - P             | Set an ADR immediate value to bits [20:0] of X; check that $-2^{20}$ <= X < $2^{20}$                                                |
| 275           | 11            | R_ <cls>_ ADR_PREL_PG_HI21</cls>        | Page(S+A)<br>-Page(P) | Set an ADRP immediate value to bits [32:12] of the X; check that $-2^{32} \le X \le 2^{32}$                                         |
| 276           | -             | R_ <cls>_<br/>ADR_PREL_PG_HI21_NC</cls> | Page(S+A)<br>-Page(P) | Set an ADRP immediate value to bits [32:12] of the X. No overflow check                                                             |
| 277           | 12            | R_ <cls>_ ADD_ABS_LO12_NC</cls>         | S + A                 | Set an ADD immediate value to bits [11:0] of X. No overflow check. Used with relocations ADR_PREL_PG_HI21 and ADR_PREL_PG_HI21_NC   |
| 278           | 13            | R_ <cls>_ LDST8_ABS_LO12_NC</cls>       | S + A                 | Set an LD/ST immediate value to bits [11:0] of X. No overflow check. Used with relocations ADR_PREL_PG_HI21 and ADR_PREL_PG_HI21_NC |
| 284           | 14            | R_ <cls>_ LDST16_ABS_LO12_NC</cls>      | S + A                 | Set an LD/ST immediate value to bits [11:1] of X. No overflow check                                                                 |

| ELF64<br>Code | ELF32<br>Code | Name                                    | Operation | Comment                                                              |
|---------------|---------------|-----------------------------------------|-----------|----------------------------------------------------------------------|
| 285           | 15            | R_ <cls>_ LDST32_ABS_LO12_NC</cls>      | S + A     | Set the LD/ST immediate value to bits [11:2] of X. No overflow check |
| 286           | 16            | R_ <cls>_ LDST64_ABS_LO12_NC</cls>      | S + A     | Set the LD/ST immediate value to bits [11:3] of X. No overflow check |
| 299           | 17            | R_ <cls>_<br/>LDST128_ABS_LO12_NC</cls> | S + A     | Set the LD/ST immediate value to bits [11:4] of X. No overflow check |

Relocations (284, 285, 286 and 299) or [14, 15, 16, 17] are intended to be used with R\_<CLS>\_ADR\_PREL\_PG\_HI21 (275) or [11] so they pick out the low 12 bits of the address and, in effect, scale that by the access size. The increased address range provided by scaled addressing is not supported by these relocations because the extra range is unusable in conjunction with R\_<CLS>\_ADR\_PREL\_PG\_HI21.

Although overflow must not be checked, a linker should check that the value of X is aligned to a multiple of the datum size.

### Relocations for control-flow instructions - all offsets are a multiple of 4

| ELF64<br>Code | ELF32<br>Code | Name                    | Operation | Comment                                                                                                           |
|---------------|---------------|-------------------------|-----------|-------------------------------------------------------------------------------------------------------------------|
| 279           | 18            | R_ <cls>_TSTBR14</cls>  | S+A-P     | Set the immediate field of a TBZ/TBNZ instruction to bits [15:2] of X; check $-2^{15}$ $<=$ X $<$ $2^{15}$        |
| 280           | 19            | R_ <cls>_CONDBR19</cls> | S+A-P     | Set the immediate field of a conditional branch instruction to bits [20:2] of X; check $-2^{20} \le X \le 2^{20}$ |
| 282           | 20            | R_ <cls>_JUMP26</cls>   | S+A-P     | Set a B immediate field to bits [27:2] of X; check that $-2^{27} \le X \le 2^{27}$                                |
| 283           | 21            | R_ <cls>_CALL26</cls>   | S+A-P     | Set a CALL immediate field to bits [27:2] of X; check that $-2^{27} \le X \le 2^{27}$                             |

### <span id="page-23-0"></span>Group relocations to create a 16, 32, 48, or 64 bit PC-relative offset inline

| ELF64<br>Code | ELF32<br>Code | Name                           | Operation | Comment                                                            |
|---------------|---------------|--------------------------------|-----------|--------------------------------------------------------------------|
| 287           | 22            | R_ <cls>_MOVW_PREL_G0</cls>    | S+A-P     | Set a MOV[NZ]immediate field to bits [15:0] of X (see notes below) |
| 288           | 23            | R_ <cls>_MOVW_PREL_G0_NC</cls> | S+A-P     | Set a MOVK immediate field to bits [15:0] of X. No overflow check  |

| ELF64<br>Code | ELF32<br>Code | Name                           | Operation | Comment                                                             |
|---------------|---------------|--------------------------------|-----------|---------------------------------------------------------------------|
| 289           | 24            | R_ <cls>_MOVW_PREL_G1</cls>    | S+A-P     | Set a MOV[NZ]immediate field to bits [31:16] of X (see notes below) |
| 290           | -             | R_ <cls>_MOVW_PREL_G1_NC</cls> | S+A-P     | Set a MOVK immediate field to bits [31:16] of X. No overflow check  |
| 291           | -             | R_ <cls>_MOVW_PREL_G2</cls>    | S+A-P     | Set a MOV[NZ]immediate value to bits [47:32] of X (see notes below) |
| 292           | -             | R_ <cls>_MOVW_PREL_G2_NC</cls> | S+A-P     | Set a MOVK immediate field to bits [47:32] of X. No overflow check  |
| 293           | -             | R_ <cls>_MOVW_PREL_G3</cls>    | S+A-P     | Set a MOV[NZ]immediate value to bits [63:48] of X (see notes below) |

Non-checking ( NC) forms relocate MOVK; checking forms relocate MOVN or MOVZ.

X >= 0: Set the instruction to MOVZ and its immediate value to the selected bits of X; for relocation R\_...\_Gn, check in ELF64 that X < {G0:  $2^{16}$ , G1:  $2^{32}$ , G2:  $2^{48}$ } (no check for R\_...\_G3); in ELF32 only check X <  $2^{16}$  for R\_...\_G0.

x < 0: Set the instruction to MOVN and its immediate value to NOT (selected bits of X); for relocation R\_...\_Gn, check in ELF64 that -{G0:  $2^{16}$ , G1:  $2^{32}$ , G2:  $2^{48}$ } <= X (no check for R\_...\_G3); in ELF32 only check that  $-2^{16}$  <= X for R\_...\_G0.

#### <span id="page-24-0"></span>Group relocations to create a 16, 32, 48, or 64 bit GOT-relative offsets inline

| ELF64<br>Code | ELF32<br>Code | Name                             | Operation          | Comment                                                                 |
|---------------|---------------|----------------------------------|--------------------|-------------------------------------------------------------------------|
| 300           | -             | R_ <cls>_MOVW_GOTOFF_G0</cls>    | G(GDAT(S))<br>-GOT | Set a MOV[NZ] immediate field to bits [15:0] of X (see notes below)     |
| 301           | -             | R_ <cls>_MOVW_GOTOFF_G0_NC</cls> | G(GDAT(S))<br>-GOT | Set a MOVK immediate field<br>to bits [15:0] of X. No<br>overflow check |
| 302           | -             | R_ <cls>_MOVW_GOTOFF_G1</cls>    | G(GDAT(S))<br>-GOT | Set a MOV[NZ] immediate value to bits [31:16] of X (see notes below)    |
| 303           | -             | R_ <cls>_MOVW_GOTOFF_G1_NC</cls> | G(GDAT(S))<br>-GOT | Set a MOVK immediate value to bits [31:16] of X. No overflow check      |
| 304           | -             | R_ <cls>_MOVW_GOTOFF_G2</cls>    | G(GDAT(S))<br>-GOT | Set a MOV[NZ] immediate value to bits [47:32] of X (see notes below)    |

| ELF64<br>Code | ELF32<br>Code | Name                             | Operation          | Comment                                                              |
|---------------|---------------|----------------------------------|--------------------|----------------------------------------------------------------------|
| 305           | -             | R_ <cls>_MOVW_GOTOFF_G2_NC</cls> | G(GDAT(S))<br>-GOT | Set a MOVK immediate value to bits [47:32] of X. No overflow check   |
| 306           | -             | R_ <cls>_MOVW_GOTOFF_G3</cls>    | G(GDAT(S))<br>-GOT | Set a MOV[NZ] immediate value to bits [63:48] of X (see notes below) |

Non-checking (\_NC) forms relocate MOVK; checking forms relocate MOVN or MOVZ.

### <span id="page-25-0"></span>GOT-relative data relocations

| ELF64<br>Code | ELF32<br>Code | Name                      | Operation     | Comment                                                                                                                                                                       |
|---------------|---------------|---------------------------|---------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 307           | -             | R_ <cls>_GOTREL64</cls>   | S+A-GOT       | Write bits [63:0] of X at byte-aligned place P. This represents a 64-bit offset relative to the GOT.                                                                          |
| 308           | -             | R_ <cls>_GOTREL32</cls>   | S+A-GOT       | Write bits [31:0] of X at byte-aligned place P. This represents a 32-bit offset relative to GOT, treated as signed; Check that $-2^{31} <= X < 2^{31}$ .                      |
| 315           | -             | R_ <cls>_GOTPCREL32</cls> | G(GDAT(S))- P | Write bits [31:0] of X at byte-aligned place P. This represents a 32-bit offset relative to GOT entry for an address, treated as signed; Check that $-2^{31} <= X < 2^{31}$ . |

### <span id="page-25-1"></span>GOT-relative instruction relocations

| ELF64<br>Code | ELF32<br>Code | Name                            | Operation       | Comment                                                                                               |
|---------------|---------------|---------------------------------|-----------------|-------------------------------------------------------------------------------------------------------|
| 309           | 25            | R_ <cls>_GOT_LD_PREL19</cls>    | G(GDAT(S))- P   | Set a load-literal immediate field to bits [20:2] of X; check -2 <sup>20</sup> <= X < 2 <sup>20</sup> |
| 310           | -             | R_ <cls>_LD64_GOTOFF_L015</cls> | G(GDAT(S))- GOT | Set a LD/ST immediate field to bits [14:3] of X; check that 0 <= X < 2 <sup>15</sup> , X&7 = 0        |

| ELF64<br>Code | ELF32<br>Code | Name                                  | Operation                    | Comment                                                                                              |
|---------------|---------------|---------------------------------------|------------------------------|------------------------------------------------------------------------------------------------------|
| 311           | 26            | R_ <cls>_ADR_GOT_PAGE</cls>           | Page(G(GDAT(S)))-<br>Page(P) | Set the immediate value<br>of an ADRP to bits<br>[32:12] of X; check that<br>–232 <= X < 232         |
| 312           | -             | R_ <cls>_LD64_GOT_LO12_NC</cls>       | G(GDAT(S))                   | Set the LD/ST<br>immediate field to bits<br>[11:3] of X. No overflow<br>check; check that X&7 =<br>0 |
| -             | 27            | R_ <cls>_LD32_GOT_LO12_NC</cls>       | G(GDAT(S))                   | Set the LD/ST<br>immediate field to bits<br>[11:2] of X. No overflow<br>check; check that X&3 =<br>0 |
| 313           | -             | R_ <cls>_LD64_GOTPAGE_LO1<br/>5</cls> | G(GDAT(S))-Page(G<br>OT)     | Set the LD/ST<br>immediate field to bits<br>[14:3] of X; check that 0<br><= X < 215, X&7 = 0         |
| -             | 28            | R_ <cls>_LD32_GOTPAGE_LO1<br/>4</cls> | G(GDAT(S))-Page(G<br>OT)     | Set the LD/ST<br>immediate field to bits<br>[13:2] of X; check that 0<br><= X < 214, X&3 = 0         |

# <span id="page-26-1"></span><span id="page-26-0"></span>5.7.7 Call and Jump relocations

There is one relocation code (R\_<CLS>\_CALL26) for function call (BL) instructions and one (R\_<CLS>\_JUMP26) for jump (B) instructions. The (R\_<CLS>\_PLT32) relocation is a data relocation for calculating the offset to a function. This can be used as the target of an indirect jump.

A linker may use a veneer (a sequence of instructions) to implement a relocated branch if the relocation is either

R\_<CLS>\_CALL26, R\_<CLS>\_JUMP26 or R\_<CLS>\_PLT32 and:

- The target symbol has type STT\_FUNC.
- Or, the target symbol and relocated place are in separate sections input to the linker.
- Or, the target symbol is undefined (external to the link unit).

In all other cases a linker shall diagnose an error if relocation cannot be effected without a veneer. A linker generated veneer may corrupt registers IP0 and IP1 [\[AAPCS64\]](https://github.com/ARM-software/abi-aa/releases) and the condition flags, but must preserve all other registers. Linker veneers may be needed for a number of reasons, including, but not limited to:

- Target is outside the addressable span of the branch instruction (+/- 128MB).
- Target address will not be known until run time, or the target address might be pre-empted.

In some systems indirect calls may also use veneers in order to support dynamic linkage that preserves pointer comparability (all reference to the function resolve to the same address).

On platforms that do not support dynamic pre-emption of symbols, an unresolved weak reference to a symbol relocated by R\_<CLS>\_CALL26 shall be treated as a jump to the next instruction (the call becomes a no-op). The behaviour of R\_<CLS>\_JUMP26 and R\_<CLS>\_PLT32 in these conditions is not specified by this standard.

## <span id="page-27-2"></span><span id="page-27-0"></span>5.7.8 Group relocations

A relocation code whose name ends in \_Gn or \_Gn\_NC (n = 0, 1, 2, 3) relocates an instruction in a group of instructions that generate a single value or address (see tables [unsigned inline group relocations](#page-21-1)[, signed](#page-21-2) [inline group relocations](#page-21-2)[, PC-relative inline relocations](#page-23-0)[, GOT-relative inline relocations\)](#page-24-0). Each such relocation relocates one instruction in isolation, with no need to determine all members of the group at link time.

These relocations operate by performing the relocation calculation then extracting a field from the result X. Generating the field for a Gn relocation directive starts by examining the residual value Yn after the bits of abs(X) corresponding to less significant fields have been masked off from X. If M is the mask specified in the table recording the relocation directive, Yn = abs(X) & ~((M & -M) - 1).

Overflow checking is performed on Yn unless the name of the relocation ends in "\_NC".

Finally the bit-field of X specified in the table (those bits of X picked out by 1-bits in M) is encoded into the instruction's literal field as specified in the table. In some cases other instruction bits may need to be changed according to the sign of X.

For "MOVW" type relocations it is the assembler's responsibility to encode the hw bits (bits 21 and 22) to indicate the bits in the target value that the immediate field represents.

### <span id="page-27-3"></span><span id="page-27-1"></span>5.7.9 Relocation optimization

Linkers may optionally optimize instructions affected by relocation. Relocation optimizations improve the efficiency of relocated instructions without changing their visible behaviour. There are several classes of relocation optimizations:

- A single relocation optimization may change an instruction after relocation into an equivalent, more efficient form.
  - Several relocations may result in an addition with zero, which may be optimized as follows:

```
ADD x0, x1, 0 // eg. R_<CLS>_TLSLE_ADD_TPREL_HI12
ADD x2, x2, 0 // or R_<CLS>_ADD_ABS_LO12_NC
// after optimization:
MOV x0, x1
NOP
```

- The relocation R\_<CLS>\_ADR\_PREL\_PG\_HI21 may emit a MOV with zero immediate for undefined weak symbols.
- The following TLS relocations may be optimized if the symbol is not a pre-emptable definition and the TLS offset fits in 16 bits:

```
ADRP x0, :gottprel: symbol // R_<CLS>_TLSIE_ADR_GOTTPREL_PAGE21
LDR x1, [x0, :gottprel_lo12: symbol] // R_<CLS>_TLSIE_LD64_GOTTPREL_LO12_NC
LDR x2, :gottprel: symbol // R_<CLS>_TLSIE_LD_GOTTPREL_PREL19
// after optimization:
NOP
MOV x1, :tprel_g0: symbol // R_<CLS>_TLSLE_MOVW_TPREL_G0
MOV x2, :tprel_g0: symbol // R_<CLS>_TLSLE_MOVW_TPREL_G0
```

If a linker supports optimizing R\_<CLS>\_TLSIE\_ADR\_GOTTPREL\_PAGE21, it must also support optimizing R\_<CLS>\_TLSIE\_LD64\_GOTTPREL\_LO12\_NC.

• A sequence of relocated instructions may be optimized if all of the following conditions are true:

- The relocations apply to consecutive instructions in the order specified.
- The relocations use the same symbol.
- The relocated instructions have the same source and destination register.
- The relocations do not appear separately or in a different order.

In this case each set of relocations is independent and may be optimized. The following sequences are defined:

• Large GOT indirection

A GOT indirection may be optimized into PC-relative addressing:

```
ADRP x0, :got: symbol // R_<CLS>_ADR_GOT_PAGE
LDR x0, [x0 :got_lo12: symbol] // R_<CLS>_LD64_GOT_LO12_NC
// after optimization:
ADRP x0, symbol // R_<CLS>_ADR_PREL_PG_HI21
ADD x0, x0, :lo12: symbol // R_<CLS>_ADD_ABS_LO12_NC
```

This sequence may be optimized if it meets all of the following conditions:

- symbol is not a pre-emptable definition.
- symbol is not of type STT\_GNU\_IFUNC.
- symbol does not have a st\_shndx of SHN\_ABS or the output is not required to be position independent.
- symbol is within range of the R\_<CLS>\_ADR\_PREL\_PG\_HI21 relocation.
- The addend of both relocations is zero.

The optimized sequence does not require a GOT entry. A linker may avoid creating a GOT entry if no other GOT relocations exist for the symbol.

• PC-relative addressing

ADR may replace ADRP/ADD if symbol is within +-1MiB range:

```
ADRP x0, symbol // R_<CLS>_ADR_PREL_PG_HI21
ADD x0, x0, :lo12: symbol // R_<CLS>_ADD_ABS_LO12_NC
// after optimization:
NOP
ADR x0, symbol // R_<CLS>_ADR_PREL_LO21
```

## <span id="page-28-0"></span>5.7.10 Proxy-generating relocations

A number of relocations generate proxy locations that are then subject to dynamic relocation. The proxies are normally gathered together in a single table, called the Global Offset Table or GOT. Table [GOT-relative](#page-24-0) [inline relocations](#page-24-0) and tabl[e GOT-relative instruction relocations](#page-25-1) list the relocations that generate proxy entries.

All of the GOT entries generated by these relocations are subject to dynamic reloc[ations \(Dynamic](#page-36-2) [relocations](#page-36-2)).

## <span id="page-29-1"></span><span id="page-29-0"></span>5.7.11 Relocations for thread-local storage

The static relocations needed to support thread-local storage in a SysV-type environment are listed in tables in the following subsections

In addition to the terms defined i[n Relocation types,](#page-18-3) the tables listing the static relocations relating to thread-local storage use the following terms in the column named Operation.

- GLDM(S) represents a consecutive pair of pointer-sized entries in the GOT for the load module index of the symbol S. The first pointer-sized entry will be relocated with R\_<CLS>\_TLS\_DTPMOD(S); the second pointer-sized entry will contain the constant 0.
- GTLSIDX(S,A) represents a consecutive pair of pointer-sized entries in the GOT. The entry contains a tls\_index structure describing the thread-local variable located at offset A from thread-local symbol S. The first pointer-sized entry will be relocated with R\_<CLS>\_TLS\_DTPMOD(S), the second pointer-sized entry will be relocated with R\_<CLS>\_TLS\_DTPREL(S+A).
- GTPREL(S+A) represents a pointer-sized entry in the GOT for the offset from the current thread pointer (TP) of the thread-local variable located at offset A from the symbol S. The entry will be relocated with R\_<CLS>\_TLS\_TPREL(S+A).
- GTLSDESC(S+A) represents a consecutive pair of pointer-sized entries in the GOT which contain a tlsdesc structure describing the thread-local variable located at offset A from thread-local symbol S. The first entry holds a pointer to the variable's TLS descriptor resolver function and the second entry holds a platform-specific offset or pointer. The pair of pointer-sized entries will be relocated with R\_<CLS>\_TLSDESC(S+A).
- LDM(S) resolves to the load module index of the symbol S.
- DTPREL(S+A) resolves to the offset from its module's TLS block of the thread local variable located at offset A from thread-local symbol S.
- TPREL(S+A) resolves to the offset from the current thread pointer (TP) of the thread local variable located at offset A from thread-local symbol S.
- TLSDESC(S+A) resolves to a contiguous pair of pointer-sized values, as created by GTLSDESC(S+A).

### <span id="page-29-2"></span>5.7.11.1 General Dynamic thread-local storage model

### General Dynamic TLS relocations

| ELF64<br>Code | ELF32<br>Code | Name                                  | Operation                         | Comment                                                                       |
|---------------|---------------|---------------------------------------|-----------------------------------|-------------------------------------------------------------------------------|
| 512           | 80            | R_ <cls>_TLSGD_<br/>ADR_PREL21</cls>  | G(GTLSIDX(S,A)) - P               | Set an ADR immediate field<br>to bits [20:0] of X; check<br>–220 <= X < 220   |
| 513           | 81            | R_ <cls>_TLSGD_<br/>ADR_PAGE21</cls>  | Page(G(GTLSIDX(S,A)) -<br>Page(P) | Set an ADRP immediate field<br>to bits [32:12] of X; check<br>–232 <= X < 232 |
| 514           | 82            | R_ <cls>_TLSGD_<br/>ADD_LO12_NC</cls> | G(GTLSIDX(S,A))                   | Set an ADD immediate field<br>to bits [11:0] of X. No<br>overflow check       |
| 515           | -             | R_ <cls>_TLSGD_<br/>MOVW_G1</cls>     | G(GTLSIDX(S,A)) - GOT             | Set a MOV[NZ] immediate<br>field to bits [31:16] of X (see<br>notes below)    |

| ELF64<br>Code | ELF32<br>Code | Name                                 | Operation             | Comment                                                                 |
|---------------|---------------|--------------------------------------|-----------------------|-------------------------------------------------------------------------|
| 516           | -             | R_ <cls>_TLSGD_<br/>MOVW_G0_NC</cls> | G(GTLSIDX(S,A)) - GOT | Set a MOVK immediate field<br>to bits [15:0] of X. No<br>overflow check |

Non-checking (\_NC) MOVW forms relocate MOVK; checking forms relocate MOVN or MOVZ.

X >= 0: Set the instruction to MOVZ and its immediate value to the selected bits of X; check that X < 2 32 .

X < 0: Set the instruction to MOVN and its immediate value to NOT (selected bits of X); check that -2<sup>32</sup> <= X.

### 5.7.11.2 Local Dynamic thread-local storage model

#### Local Dynamic TLS relocations

| ELF64<br>Code | ELF32<br>Code | Name                                     | Operation                    | Comment                                                                             |
|---------------|---------------|------------------------------------------|------------------------------|-------------------------------------------------------------------------------------|
| 517           | 83            | R_ <cls>_TLSLD_<br/>ADR_PREL21</cls>     | G(GLDM(S))) - P              | Set an ADR immediate field<br>to bits [20:0] of X; check<br>–220 <= X < 220         |
| 518           | 84            | R_ <cls>_TLSLD_<br/>ADR_PAGE21</cls>     | Page(G(GLDM(S)))-Page(P<br>) | Set an ADRP immediate field<br>to bits [32:12] of X; check<br>–232 <= X < 232       |
| 519           | 85            | R_ <cls>_TLSLD_<br/>ADD_LO12_NC</cls>    | G(GLDM(S))                   | Set an ADD immediate field<br>to bits [11:0] of X. No<br>overflow check             |
| 520           | -             | R_ <cls>_TLSLD_<br/>MOVW_G1</cls>        | G(GLDM(S)) - GOT             | Set a MOV[NZ] immediate<br>field to bits [31:16] of X (see<br>notes below)          |
| 521           | -             | R_ <cls>_TLSLD_<br/>MOVW_G0_NC</cls>     | G(GLDM(S)) - GOT             | Set a MOVK immediate field<br>to bits [15:0] of X. No<br>overflow check             |
| 522           | 86            | R_ <cls>_TLSLD_<br/>LD_PREL19</cls>      | G(GLDM(S)) - P               | Set a load-literal immediate<br>field to bits [20:2] of X;<br>check –220 <= X < 220 |
| 523           | -             | R_ <cls>_TLSLD_<br/>MOVW_DTPREL_G2</cls> | DTPREL(S+A)                  | Set a MOV[NZ] immediate<br>field to bits [47:32] of X (see<br>notes below)          |
| 524           | 87            | R_ <cls>_TLSLD_<br/>MOVW_DTPREL_G1</cls> | DTPREL(S+A)                  | Set a MOV[NZ] immediate<br>field to bits [31:16] of X (see<br>notes below)          |

| ELF64<br>Code | ELF32<br>Code | Name                                                  | Operation   | Comment                                                                   |
|---------------|---------------|-------------------------------------------------------|-------------|---------------------------------------------------------------------------|
| 525           | -             | R_ <cls>_TLSLD_ MO<br/>VW_DTPREL_G1_NC</cls>          | DTPREL(S+A) | Set a MOVK immediate field<br>to bits [31:16] of X. No<br>overflow check  |
| 526           | 88            | R_ <cls>_TLSLD_<br/>MOVW_DTPREL_G0</cls>              | DTPREL(S+A) | Set a MOV[NZ] immediate<br>field to bits [15:0] of X (see<br>notes below) |
| 527           | 89            | R_ <cls>_TLSLD_ MO<br/>VW_DTPREL_G0_NC</cls>          | DTPREL(S+A) | Set a MOVK immediate field<br>to bits [15:0] of X. No<br>overflow check   |
| 528           | 90            | R_ <cls>_TLSLD_<br/>ADD_DTPREL_HI12</cls>             | DTPREL(S+A) | Set an ADD immediate field<br>to bits [23:12] of X; check 0<br><= X < 224 |
| 529           | 91            | R_ <cls>_TLSLD_<br/>ADD_DTPREL_LO12</cls>             | DTPREL(S+A) | Set an ADD immediate field<br>to bits [11:0] of X; check 0<br><= X < 212  |
| 530           | 92            | R_ <cls>_TLSLD_ AD<br/>D_DTPREL_LO12_NC</cls>         | DTPREL(S+A) | Set an ADD immediate field<br>to bits [11:0] of X. No<br>overflow check   |
| 531           | 93            | R_ <cls>_TLSLD_<br/>LDST8_DTPREL_LO12</cls>           | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:0] of X; check 0 <=<br>X < 212    |
| 532           | 94            | R_ <cls>_TLSLD_ LD<br/>ST8_DTPREL_LO12_N<br/>C</cls>  | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:0] of X. No overflow<br>check     |
| 533           | 95            | R_ <cls>_TLSLD_ LD<br/>ST16_DTPREL_LO12</cls>         | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:1] of X; check 0 <=<br>X < 212    |
| 534           | 96            | R_ <cls>_TLSLD_ LD<br/>ST16_DTPREL_LO12_<br/>NC</cls> | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:1] of X. No overflow<br>check     |
| 535           | 97            | R_ <cls>_TLSLD_ LD<br/>ST32_DTPREL_LO12</cls>         | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:2] of X; check 0 <=<br>X < 212    |
| 536           | 98            | R_ <cls>_TLSLD_ LD<br/>ST32_DTPREL_LO12_<br/>NC</cls> | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:2] of X. No overflow<br>check     |
| 537           | 99            | R_ <cls>_TLSLD_ LD<br/>ST64_DTPREL_LO12</cls>         | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:3] of X; check 0 <=<br>X < 212    |
| 538           | 100           | R_ <cls>_TLSLD_ LD<br/>ST64_DTPREL_LO12_<br/>NC</cls> | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:3] of X. No overflow<br>check     |
| 572           | 101           | R_ <cls>_TLSLD_ LD<br/>ST128_DTPREL_LO12</cls>        | DTPREL(S+A) | Set a LD/ST offset field to<br>bits [11:4] of X; check 0 <=<br>X < 212    |

| ELF64<br>Code | ELF32<br>Code | Name                                                    | Operation   | Comment                                                         |
|---------------|---------------|---------------------------------------------------------|-------------|-----------------------------------------------------------------|
| 573           | 102           | R_ <cls>_TLSLD_ LD<br/>ST128_DTPREL_LO12<br/>_ NC</cls> | DTPREL(S+A) | Set a LD/ST offset field to bits [11:4] of X. No overflow check |

Non-checking ( NC) MOVW forms relocate MOVK; checking forms relocate MOVN or MOVZ.

<code>X >= 0:</code> Set the instruction to <code>MOVZ</code> and its immediate value to the selected bits S; for relocation <code>R\_...\_Gn</code>, check in ELF64 that <code>X < {G0: 2^{16}, G1: 2^{32}, G2: 2^{48}}</code> (no check for <code>R\_...\_G3</code>); in ELF32 only check that <code>X < 2^{16}</code> for <code>R\_...\_G0</code>.

X < 0: Set the instruction to MOVN and its immediate value to NOT (selected bits of); for relocation R\_...\_Gn, check in ELF64 that -{G0:  $2^{16}$ , G1:  $2^{32}$ , G2:  $2^{48}$ } <= X (no check for R\_...\_G3); in ELF32 only check that - $2^{16}$  <= X for R\_...\_G0.

For scaled-addressing relocations (533-538, 572 and 573) or [95-102] a linker should check that X is a multiple of the datum size.

### 5.7.11.3 Initial Exec thread-local storage model

#### Initial Exec TLS relocations

| ELF64<br>Code | ELF32<br>Code | Name                                             | Operation                         | Comment                                                                           |
|---------------|---------------|--------------------------------------------------|-----------------------------------|-----------------------------------------------------------------------------------|
| 539           | -             | R_ <cls>_TLSIE_<br/>MOVW_GOTTPREL_G1</cls>       | G(GTPREL(S+A)) - GOT              | Set a MOV[NZ]<br>immediate field to bits<br>[31:16] of X (see notes<br>below)     |
| 540           | -             | R_ <cls>_TLSIE_<br/>MOVW_GOTTPREL_G0_NC</cls>    | G(GTPREL(S+A)) - GOT              | Set MOVK immediate to bits [15:0] of X. No overflow check                         |
| 541           | 103           | R_ <cls>_TLSIE_<br/>ADR_GOTTPREL_PAGE21</cls>    | Page(G(GTPREL(S+A))) -<br>Page(P) | Set an ADRP immediate field to bits [32:12] of X; check $-2^{32}$ <= X < $2^{32}$ |
| 542           | -             | R_ <cls>_TLSIE_ LD64_G<br/>OTTPREL_LO12_NC</cls> | G(GTPREL(S+A))                    | Set an LD offset field to bits [11:3] of X. No overflow check; check that X&7=0   |
| -             | 104           | R_ <cls>_TLSIE_ LD32_G<br/>OTTPREL_LO12_NC</cls> | G(GTPREL(S+A))                    | Set an LD offset field to bits [11:2] of X. No overflow check; check that X&3=0   |
| 543           | 105           | R_ <cls>_TLSIE_<br/>LD_GOTTPREL_PREL19</cls>     | G(GTPREL(S+A)) - P                | Set a load-literal immediate to bits [20:2] of X; check $-2^{20} <= X < 2^{20}$   |

Non-checking (\_NC) MOVW forms relocate MOVK; checking forms relocate MOVN or MOVZ.

### 5.7.11.4 Local Exec thread-local storage model

### Local Exec TLS relocations

| ELF64<br>Code | ELF32<br>Code | Name                                    | Operation  | Comment                                                                       |
|---------------|---------------|-----------------------------------------|------------|-------------------------------------------------------------------------------|
| 544           | -             | R_ <cls>_TLSLE_ MOVW_TPREL_G2</cls>     | TPREL(S+A) | Set a MOV[NZ]<br>immediate field to<br>bits [47:32] of X<br>(see notes below) |
| 545           | 106           | R_ <cls>_TLSLE_ MOVW_TPREL_G1</cls>     | TPREL(S+A) | Set a MOV[NZ]<br>immediate field to<br>bits [31:16] of X<br>(see notes below) |
| 546           | -             | R_ <cls>_TLSLE_ MOVW_TPREL_G1_NC</cls>  | TPREL(S+A) | Set a MOVK<br>immediate field to<br>bits [31:16] of X. No<br>overflow check   |
| 547           | 107           | R_ <cls>_TLSLE_ MOVW_TPREL_G0</cls>     | TPREL(S+A) | Set a MOV[NZ]<br>immediate field to<br>bits [15:0] of X (see<br>notes below)  |
| 548           | 108           | R_ <cls>_TLSLE_ MOVW_TPREL_G0_NC</cls>  | TPREL(S+A) | Set a MOVK<br>immediate field to<br>bits [15:0] of X. No<br>overflow check    |
| 549           | 109           | R_ <cls>_TLSLE_ ADD_TPREL_HI12</cls>    | TPREL(S+A) | Set an ADD<br>immediate field to<br>bits [23:12] of X;<br>check 0 <= X < 224  |
| 550           | 110           | R_ <cls>_TLSLE_ ADD_TPREL_LO12</cls>    | TPREL(S+A) | Set an ADD<br>immediate field to<br>bits [11:0] of X;<br>check 0 <= X < 212   |
| 551           | 111           | R_ <cls>_TLSLE_ ADD_TPREL_LO12_NC</cls> | TPREL(S+A) | Set an ADD<br>immediate field to<br>bits [11:0] of X. No<br>overflow check    |
| 552           | 112           | R_ <cls>_TLSLE_ LDST8_TPREL_LO12</cls>  | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:0]<br>of X; check 0 <= X<br>< 212     |

| ELF64<br>Code | ELF32<br>Code | Name                                            | Operation  | Comment                                                                   |
|---------------|---------------|-------------------------------------------------|------------|---------------------------------------------------------------------------|
| 553           | 113           | R_ <cls>_TLSLE_ LDST8_TPREL_LO12_NC</cls>       | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:0]<br>of X. No overflow<br>check  |
| 554           | 114           | R_ <cls>_TLSLE_ LDST16_TPREL_LO12</cls>         | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:1]<br>of X; check 0 <= X<br>< 212 |
| 555           | 115           | R_ <cls>_TLSLE_ LDST16_TPREL_LO12_NC</cls>      | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:1]<br>of X. No overflow<br>check  |
| 556           | 116           | R_ <cls>_TLSLE_ LDST32_TPREL_LO12</cls>         | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:2]<br>of X; check 0 <= X<br>< 212 |
| 557           | 117           | R_ <cls>_TLSLE_ LDST32_TPREL_LO12_NC</cls>      | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:2]<br>of X. No overflow<br>check  |
| 558           | 118           | R_ <cls>_TLSLE_ LDST64_TPREL_LO12</cls>         | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:3]<br>of X; check 0 <= X<br>< 212 |
| 559           | 119           | R_ <cls>_TLSLE_ LDST64_TPREL_LO12_NC</cls>      | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:3]<br>of X. No overflow<br>check  |
| 570           | 120           | R_ <cls>_TLSLE_ LDST128_TPREL_LO12</cls>        | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:4]<br>of X; check 0 <= X<br>< 212 |
| 571           | 121           | R_ <cls>_TLSLE_<br/>LDST128_TPREL_LO12_NC</cls> | TPREL(S+A) | Set a LD/ST offset<br>field to bits [11:4]<br>of X. No overflow<br>check  |

Non-checking (\_NC) MOVW forms relocate MOVK; checking forms relocate MOVN or MOVZ.

For scaled-addressing relocations (554-559, 570 and 571) or [112-121] a linker should check that X is a multiple of the datum size.

### <span id="page-34-0"></span>5.7.11.5 Thread-local storage descriptors

#### TLS descriptor relocations

| ELF64<br>Code | ELF32<br>Code | Name                                    | Operation                        | Comment                                                                                                                                                    |
|---------------|---------------|-----------------------------------------|----------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 560           | 122           | R_ <cls>_TLSDESC<br/>_ LD_PREL19</cls>  | G(GTLSDESC(S+A)) - P             | Set a load-literal immediate to bits [20:2]; check $-2^{20} \le X \le 2^{20}$ ; check X & 3 = 0.                                                           |
| 561           | 123           | R_ <cls>_TLSDESC<br/>_ ADR_PREL21</cls> | G(GTLSDESC(S+A)) - P             | Set an ADR immediate field to bits [20:0]; check $-2^{20} \le X \le 2^{20}$ .                                                                              |
| 562           | 124           | R_ <cls>_TLSDESC<br/>_ ADR_PAGE21</cls> | Page(G(GTLSDESC(S+A))) - Page(P) | Set an ADRP immediate field to bits [32:12] of X; check $-2^{32}$ $<= X < 2^{32}$ .                                                                        |
| 563           | -             | R_ <cls>_TLSDESC<br/>_ LD64_LO12</cls>  | G(GTLSDESC(S+A))                 | Set an LD offset field to bits [11:3] of X. No overflow check; check X & 7 = 0.                                                                            |
| -             | 125           | R_ <cls>_TLSDESC<br/>_ LD32_LO12</cls>  | G(GTLSDESC(S+A))                 | Set an LD offset field to bits [11:2] of X. No overflow check; check X & 3 = 0.                                                                            |
| 564           | 126           | R_ <cls>_TLSDESC<br/>_ ADD_LO12</cls>   | G(GTLSDESC(S+A))                 | Set an ADD immediate field to bits [11:0] of X. No overflow check.                                                                                         |
| 565           | -             | R_ <cls>_TLSDESC<br/>_ OFF_G1</cls>     | G(GTLSDESC(S+A)) - GOT           | Set a MOV[NZ] immediate field to bits [31:16] of X; check -2 <sup>32</sup> <= X < 2 <sup>32</sup> . See notes below.                                       |
| 566           | -             | R_ <cls>_TLSDESC<br/>_ OFF_G0_NC</cls>  | G(GTLSDESC(S+A)) - GOT           | Set a MOVK immediate field to bits [15:0] of X. No overflow check.                                                                                         |
| 567           | -             | R_ <cls>_TLSDESC<br/>_ LDR</cls>        | None                             | For relaxation only. Must be used to identify an LDR instruction which loads the TLS descriptor function pointer for S + A if it has no other relocation.  |
| 568           | -             | R_ <cls>_TLSDESC<br/>_ ADD</cls>        | None                             | For relaxation only. Must be used to identify an ADD instruction which computes the address of the TLS Descriptor for S + A if it has no other relocation. |
| 569           | 127           | R_ <cls>_TLSDESC<br/>_ CALL</cls>       | None                             | For relaxation only. Must be used to identify a BLR instruction which performs an indirect call to the TLS descriptor function for S + A.                  |

 $X \ge 0$ : Set the instruction to MOVZ and its immediate value to the selected bits of X.

X < 0: Set the instruction to MOVN and its immediate value to NOT (selected bits of X).

Relocation codes R\_<CLS>\_TLSDESC\_LDR, R\_<CLS>\_TLSDESC\_ADD and R\_<CLS>\_TLSDESC\_CALL are needed to permit linker optimization of TLS descriptor code sequences to use Initial-exec or Local-exec TLS sequences; this can only be done if all relevant uses of TLS descriptors are marked to permit accurate relaxation. Object producers that are unable to satisfy this requirement must generate traditional General-dynamic TLS sequences using the relocations described in [General Dynamic thread-local storage](#page-29-2) [model.](#page-29-2) The details of TLS descriptors are beyond the scope of this specification; a general introduction can be found in [\[TLSDESC\]](http://www.fsfla.org/~lxoliva/writeups/TLS/paper-lk2006.pdf).

## <span id="page-36-0"></span>5.7.12 Relocations for PAuth ABI Extension

The PAuth ABI Extension defines a number of static and dynamic relocations. The information in this document is sufficient to reserve the relocation types. For details on the relocations and operations see [PAUTHABIELF64.](https://github.com/ARM-software/abi-aa/releases)

### PAuthABI static relocations

| ELF64<br>Code | ELF32<br>Code | Name                           | Operation  | Comment           |
|---------------|---------------|--------------------------------|------------|-------------------|
| 580           | -             | R_ <cls>_AUTH_AB<br/>S64</cls> | PAUTH(S+A) | See PAUTHABIELF64 |

### <span id="page-36-2"></span><span id="page-36-1"></span>5.7.13 Dynamic relocations

The dynamic relocations for those execution environments that support only a limited number of run-time relocation types are listed in the below table. The enumeration of dynamic relocations commences at (1024) or [180] and the range is compact.

#### Dynamic relocations

| ELF64<br>Code | ELF32<br>Code | Name                       | Operation                  | Comment         |
|---------------|---------------|----------------------------|----------------------------|-----------------|
| 257           | -             | R_ <cls>_ABS64</cls>       | S + A                      | See note below. |
| -             | 1             | R_ <cls>_ABS32</cls>       | S + A                      | See note below. |
| 580           | -             | R_ <cls>_AUTH_ABS64</cls>  | SIGN(S + A,<br>SCHEMA(*P)) | See note below. |
| 1024          | 180           | R_ <cls>_COPY</cls>        |                            | See note below. |
| 1025          | 181           | R_ <cls>_GLOB_DAT</cls>    | S + A                      | See note below  |
| 1026          | 182           | R_ <cls>_JUMP_SLOT</cls>   | S + A                      | See note below  |
| 1027          | 183           | R_ <cls>_RELATIVE</cls>    | Delta(S) + A               | See note below  |
| 1028          | 184           | R_ <cls>_TLS_IMPDEF1</cls> |                            | See note below  |
| 1029          | 185           | R_ <cls>_TLS_IMPDEF2</cls> |                            | See note below  |
|               |               | R_ <cls>_TLS_DTPREL</cls>  | DTPREL(S+A)                | See note below  |
|               |               | R_ <cls>_TLS_DTPMOD</cls>  | LDM(S)                     | See note below  |
| 1030          | 186           | R_ <cls>_TLS_TPREL</cls>   | TPREL(S+A)                 |                 |

| ELF64<br>Code | ELF32<br>Code | Name                              | Operation                         | Comment                                     |
|---------------|---------------|-----------------------------------|-----------------------------------|---------------------------------------------|
| 1031          | 187           | R_ <cls>_TLSDESC</cls>            | TLSDESC(S+A)                      | Identifies a TLS descriptor to<br>be filled |
| 1032          | 188           | R_ <cls>_IRELATIVE</cls>          | Indirect(Delta(S) +<br>A)         | See note below.                             |
| 1041          | -             | R_ <cls>_AUTH_RELATI<br/>VE</cls> | SIGN(DELTA(S) + A,<br>SCHEMA(*P)) | See note below.                             |

With the exception of R\_<CLS>\_COPY all dynamic relocations require that the place being relocated is an 8-byte aligned 64-bit data location in ELF64 or a 4-byte aligned 32-bit data location in ELF32.

R\_<CLS>\_ABS64 and R\_<CLS>\_ABS32 may only appear in a well-formed executable or dynamic shared object in ELF64 or ELF32 respectively. Note that for their respective file format these relocations are both static and dynamic relocations.

R\_<CLS>\_COPY may only appear in executable ELF files where e\_type is set to ET\_EXEC. The effect is to cause the dynamic linker to locate the target symbol in a shared library object and then to copy the number of bytes specified by its st\_size field to the place. The address of the place is then used to pre-empt all other references to the specified symbol. It is an error if the storage space allocated in the executable is insufficient to hold the full copy of the symbol. If the object being copied contains dynamic relocations then the effect must be as if those relocations were performed before the copy was made.

R\_<CLS>\_COPY is normally only used in SysV type environments where the executable is not positionindependent and references by the code and read-only data sections cannot be relocated dynamically to refer to an object that is defined in a shared library.

The need for copy relocations can be avoided if a compiler generates all code references to such objects indirectly through a dynamically relocatable location and if all static data references are placed in relocatable regions of the image. In practice, this is difficult to achieve without source-code annotation. A better approach is to avoid defining static global data in shared libraries.

R\_<CLS>\_GLOB\_DAT relocates a GOT entry used to hold the address of a (data) symbol which must be resolved at load time.

R\_<CLS>\_JUMP\_SLOT is used to mark code targets that will be executed.

- On platforms that support dynamic binding the relocations may be performed lazily on demand.
- The initial value stored in the place is the offset to the entry sequence stub for the dynamic linker. It must be adjusted during initial loading by the offset of the load address of the segment from its link address.
- Addresses stored in the place of these relocations may not be used for pointer comparison until after the relocation has been resolved.
- Because the initial value of the place is not related to the ultimate target of a R\_<CLS>\_JUMP\_SLOT relocation the addend A of such a REL-type relocation shall be zero rather than the initial content of the place. A platform ABI shall prescribe whether or not the r\_addend field of such a RELA-type relocation is honored. (There may be security-related reasons not to do so).

R\_<CLS>\_RELATIVE represents a relative adjustment to the place based on the load address of the object relative to its original link address. All symbols defined in the same segment will have the same relative adjustment. If S is the null symbol (ELF symbol index 0) then the adjustment is based on the segment defining the place. On systems where all segments are mapped contiguously the adjustment will be the same for each relocation, thus adjustment never needs to resolve the symbol. This relocation represents an optimization; a static linker can use it to replace R\_<CLS>\_GLOB\_DAT when the symbol is known at static link time to always resolve to the current link unit.

R\_<CLS>\_IRELATIVE represents a dynamic selection of the place's resolved value. The means by which this relocation is generated is platform specific, as are the conditions that must hold when resolving takes place.

Relocations R\_AARCH64\_TLS\_DTPREL, R\_AARCH64\_TLS\_DTPMOD and R\_AARCH64\_TLS\_TPREL were previously documented as R\_AARCH64\_TLS\_DTPREL64, R\_AARCH64\_TLS\_DTPMOD64 and R\_AARCH64\_TLS\_TPREL64 respectively. The old names can be supported if needed for backwards compatibility.

It is implementation defined whether R\_<CLS>\_TLS\_IMPDEF1 implements R\_<CLS>\_TLS\_DTPREL and R\_<CLS>\_TLS\_IMPDEF2 implements R\_<CLS>\_TLS\_DTPMOD or whether R\_<CLS>\_TLS\_IMPDEF1 implements R\_<CLS>\_TLS\_DTPMOD and R\_<CLS>\_TLS\_IMPDEF2 implements R\_<CLS>\_TLS\_DTPREL; a platform must document its choice[1](#page-41-1) .

<span id="page-38-4"></span>R\\_<CLS>\\_AUTH\\_ABS64 and R\\_<CLS>\\_AUTH\\_RELATIVE are part of the PAuth ABI Extension. For details on the relocations and operations see [PAUTHABIELF64.](https://github.com/ARM-software/abi-aa/releases) Note that R\\_<CLS>\\_AUTH\\_ABS64 is both a static and a dynamic relocation.

### <span id="page-38-0"></span>5.7.14 Private and platform-specific relocations

Private relocations for vendor experiments:

- 0xE000 to 0xEFFF for ELF64
- 0xE0 to 0xEF for ELF32

Platform ABI defined relocations:

- 0xF000 to 0xFFFF for ELF64
- 0xF0 to 0xFF for ELF32

Platform ABI relocations can only be interpreted when the EI\_OSABI field is set to indicate the Platform ABI governing the definition.

All of the above codes will not be assigned by any future version of this standard.

### <span id="page-38-1"></span>5.7.15 Unallocated relocations

All unallocated relocation types are reserved for use by future revisions of this specification.

### <span id="page-38-2"></span>5.7.16 PAuthABI relocations

The [PAuthABIELF64](https://github.com/ARM-software/abi-aa/releases) ELF extension, currently in Alpha state, defines several relocations in the vendor experiment space. Arm reserves codes 580 to 600 for static [PAuthABIELF64](https://github.com/ARM-software/abi-aa/releases) relocations and 1040 - 1060 for dynamic [PAuthABIELF64](https://github.com/ARM-software/abi-aa/releases) relocations. When the extension moves to Release state the relocations defined in [PAuthABIELF64](https://github.com/ARM-software/abi-aa/releases) will be added to this document and all unused codes in the reserved ranges will be released.

### <span id="page-38-3"></span>5.7.17 Idempotency

All RELA type relocations are idempotent. They may be reapplied to the place and the result will be the same. This allows a static linker to preserve full relocation information for an image by converting all REL type relocations into RELA type relocations.

### Note

A REL type relocation can only be idempotent if the original addend was zero and if subsequent re-linking assumes that REL relocations have zero for all addends.

# <span id="page-39-7"></span><span id="page-39-0"></span>6 Program Loading and Dynamic Linking

This section provides details of AArch64-specific definitions and changes relating to executable images.

# <span id="page-39-1"></span>6.1 Program Header

The Program Header provides a number of fields that assist in interpretation of the file. Most of these are specified in the base standard [[SCO-ELF](http://www.sco.com/developers/gabi/)]. The following fields have AArch64-specific meanings.

#### **p\_type**

The below table lists the processor-specific segment types.

### Processor-specific segment types

| Name                      | p_type         | Meaning                                              |  |
|---------------------------|----------------|------------------------------------------------------|--|
| PT_AARCH64_ARCHEXT        | 0x7000000<br>0 | Reserved for architecture compatibility information  |  |
| PT_AARCH64_UNWIND         | 0x7000000<br>1 | Reserved for exception unwinding tables              |  |
| PT_AARCH64_MEMTAG_M<br>TE | 0x7000000<br>2 | Reserved for MTE memory tag data dumps in core files |  |

A segment of type PT\_AARCH64\_ARCHEXT (if present) contains information describing the architecture capabilities required by the executable file. Not all platform ABIs require this segment; the Linux ABI does not. If the segment is present it must appear before segment of type PT\_LOAD.

PT\_AARCH64\_UNWIND (if present) describes the location of a program's exception unwind tables.

PT\_AARCH64\_MEMTAG\_MTE segments (if present) hold MTE memory tags for a particular memory range. At present they are defined for core dump files of type ET\_CORE. A description of the program header and contents can be found in [\[MTEEXTENSIONS\]](https://www.kernel.org/doc/html/latest/arm64/memory-tagging-extension.html#core-dump-support).

### **p\_flags**

There are no AArch64-specific flags.

### <span id="page-39-2"></span>6.1.1 Platform architecture compatibility data

At this time this ABI specifies no generic platform architecture compatibility data.

# <span id="page-39-6"></span><span id="page-39-3"></span>6.2 Program Property

The information on Program Property has moved to [[SYSVABI64\]](https://github.com/ARM-software/abi-aa/releases).

# <span id="page-39-8"></span><span id="page-39-4"></span>6.3 Program Loading

The information on program loading has moved to [[SYSVABI64\]](https://github.com/ARM-software/abi-aa/releases).

# <span id="page-39-9"></span><span id="page-39-5"></span>6.4 Dynamic Linking

The information on Dynamic Linking has moved to [[SYSVABI64\]](https://github.com/ARM-software/abi-aa/releases).

# <span id="page-40-2"></span><span id="page-40-0"></span>6.5 Dynamic Section

The information on the Dynamic Section has moved to [[SYSVABI64](https://github.com/ARM-software/abi-aa/releases)].

# <span id="page-40-1"></span>6.5.1 Custom PLTs

The information on custom PLTs has moved to [[SYSVABI64\]](https://github.com/ARM-software/abi-aa/releases).

# <span id="page-41-0"></span>7 Footnotes

<span id="page-41-1"></span>[1](#page-38-4) Earlier versions of this specification required that R\_<CLS>\_TLS\_IMPDEF1 implement R\_<CLS>\_TLS\_DTPREL and R\_<CLS>\_TLS\_IMPDEF2 implement R\_<CLS>\_TLS\_DTPMOD; however the Linux platform ABI has always implemented the alternative specification. It is recommended that new platforms follow the Linux platform specification as this is the most widely adopted.