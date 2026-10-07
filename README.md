# BB10 Qualcomm NV Tool Documentation

**Version:** 1.4.0  
**Application Name:** BB10 Qualcomm NV Tool  

---

> ### ⚠️ CRITICAL WARNING & DISCLAIMER
> 
> **USE AT YOUR OWN RISK.** This software modifies low-level non-volatile (NV) memory parameters, modem configuration files (EFS), and IMS settings on Qualcomm chipsets. Improper modifications can lead to **permanent hardware malfunction, baseband/modem corruption, loss of cellular connectivity, loss of IMEI, or total bricking of the device.**
> 
> By using this software, you explicitly agree that:
> 1. You assume **full responsibility** for any actions performed using this tool and any consequences thereof.
> 2. Neither the author(s) nor contributors shall be held liable for any damage, data loss, or device failure resulting from the operation of this utility.
> 3. You are strongly advised to create a full system and NV backup prior to performing any write or restore operations.

---

## Overview

The **BB10 Qualcomm NV Tool** is a command-line utility designed for BlackBerry 10 devices powered by Qualcomm chipsets. It allows developers and technicians to read, write, back up, and restore non-volatile (NV) memory items, IMS parameters, and Embedded File System (EFS) files on the modem subsystem.

## Technical Specifications

| Parameter | Value / Description | 
 | ----- | ----- | 
| **Max NV Items Scanned** | `80,000` (`0` through `79,999`) | 
| **Standard NV Buffer Size** | `4096 bytes` (4 KB) | 
| **Max Allocation Buffer** | `8192 bytes` (8 KB for batch restore operations) | 
| **Large File Threshold (EFS)** | `2048 bytes` (`0x800`) | 

## Usage Syntax

```
<binary_name> <command> <id/path> [value] [-f <file>]
```

### Commands Summary

| Command | Category | Target Input | Value / Source | Description | 
 | ----- | ----- | ----- | ----- | ----- | 
| `read` | NV | Numeric ID | Terminal or `-f <file>` | Reads a specific NV item by ID and outputs hex dump or saves to file. | 
| `write` | NV | Numeric ID | HEX string or `-f <file>` | Writes raw bytes or binary file content to an NV item. | 
| `iread` | IMS | Numeric ID | Terminal or `-f <file>` | Reads an IMS parameter by ID. | 
| `iwrite` | IMS | Numeric ID | String or `-f <file>` | Writes a string or file content to an IMS parameter. | 
| `fread` | EFS | File Path | Terminal or `-f <file>` | Reads a file from the modem EFS path. | 
| `fwrite` | EFS | File Path | HEX string or `-f <file>` | Writes raw bytes or file content to an EFS file path. | 
| `backup` | Batch | Directory Name | N/A | Dumps all existing NV items (`0` to `79999`) into individual `.nv` files in the specified directory. | 
| `restore` | Batch | Directory Name | N/A | Writes all `.nv` files found in the directory back to the modem's NV memory. | 

## Command Examples

### 1. NV Item Operations

* **Read NV Item 550 to Terminal:**
  ```
  ./qc_nv_tool read 550
  ```

* **Read NV Item 550 to Binary File:**
  ```
  ./qc_nv_tool read 550 -f nv_550.bin
  ```

* **Write HEX payload to NV Item 550:**
  ```
  ./qc_nv_tool write 550 01A2FF00
  ```

* **Write File Content to NV Item 550:**
  ```
  ./qc_nv_tool write 550 -f nv_550.bin
  ```

### 2. IMS Operations

* **Read IMS Parameter 1:**
  ```
  ./qc_nv_tool iread 1
  ```

* **Write IMS Parameter 1:**
  ```
  ./qc_nv_tool iwrite 1 "ims.domain.com"
  ```

### 3. EFS Operations

* **Read EFS File:**
  ```
  ./qc_nv_tool fread /nv/item_files/modem/lte/ue_cap
  ```

* **Write File to EFS Path:**
  ```
  ./qc_nv_tool fwrite /nv/item_files/modem/lte/ue_cap -f ue_cap_config.bin
  ```

### 4. Bulk Backup & Restore

* **Back Up All NV Items:**
  ```
  ./qc_nv_tool backup ./nv_backup_folder
  ```

* **Restore All NV Items:**
  ```
  ./qc_nv_tool restore ./nv_backup_folder
  ```
