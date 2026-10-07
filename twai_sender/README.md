# ESP32-S3 ↔ STM32F103 qua CAN cổ điển

Project này chạy trên ESP-IDF 5.5.3 và kiểm tra trao đổi hai chiều với STM32F103. ESP32-S3 gửi một khung mỗi giây; STM32 trả lại đúng 8 byte. ESP in `PASS` khi nhận được phản hồi hợp lệ. Mã STM32 ở `stm32/can_echo.c` dùng STM32Cube HAL.

## Phần cứng và nối chân

Ví dụ STM32 dùng Blue Pill STM32F103C8T6, CAN1 remap sang PB8/PB9 như hình tham khảo. **Mỗi MCU cần một transceiver CAN riêng**; MCP2551 chỉ là transceiver, không phải bộ điều khiển CAN. ESP32-S3 và STM32F103 đều đã có bộ điều khiển CAN/TWAI trong chip.

| ESP32-S3 | MCP2551 phía ESP | STM32F103 | MCP2551 phía STM |
| --- | --- | --- | --- |
| GPIO4 (TWAI TX) | TXD/CTX | PB9 (CAN TX) | TXD/CTX |
| GPIO5 (TWAI RX) | RXD/CRX **qua chuyển mức 5 V → 3,3 V** | PB8 (CAN RX) | RXD/CRX **qua chuyển mức 5 V → 3,3 V** |
| GND | GND | GND | GND |
| nguồn 5 V ổn định | VDD/VCC 5 V | nguồn 5 V ổn định | VDD/VCC 5 V |

Nối `CANH ↔ CANH`, `CANL ↔ CANL`, và nối chung GND hai phía. Đặt **một điện trở 120 Ω giữa CANH và CANL ở mỗi đầu vật lý** của đường bus; khi tắt nguồn, đo giữa CANH và CANL khoảng 60 Ω. Nếu module đã gắn sẵn 120 Ω, không gắn thêm. Chân `RS` của MCP2551 cần ở chế độ high speed (theo sơ đồ module; với IC trần nối GND). Không cấp MCP2551 bằng 3,3 V. RXD của MCP2551 cấp 5 V có thể lên mức 5 V: phải dùng bộ chuyển mức phù hợp hoặc cầu phân áp, ví dụ 10 kΩ từ RXD đến GPIO RX và 20 kΩ từ GPIO RX xuống GND. Không nối RXD 5 V trực tiếp vào ESP32-S3 hay STM32. Nếu module của bạn đã có chuyển mức, xác nhận bằng sơ đồ/đo điện áp trước khi nối.

GPIO4/5 được cấu hình trong `idf.py menuconfig` → Example Configuration. PB8/PB9 là CAN remap trên STM32F103; nếu chọn PA11/PA12 thì đổi cấu hình CubeMX và dây tương ứng. Tránh dùng PA11/PA12 khi USB đang sử dụng chúng.

## Cấu hình và nạp ESP32-S3

```bash
source /home/nguyentiem/.espressif/v5.5.3/esp-idf/export.sh
idf.py set-target esp32s3
idf.py menuconfig   # kiểm tra GPIO TX=4, RX=5
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Chỉnh tên cổng serial theo máy. Bitrate cố định 500 kbit/s, CAN 2.0 standard ID, DLC 8. Hai ID: request `0x100`, response `0x101`. Byte 0–3 là số thứ tự little endian; byte 4–7 là ASCII `CAN!`. ESP đợi phản hồi trùng toàn bộ 8 byte tối đa 800 ms. Một node đơn lẻ không thể tự ACK khung CAN ở normal mode.

## Tạo firmware STM32 với STM32CubeIDE/CubeMX

1. Tạo project đúng mã chip (ví dụ STM32F103C8T6). Cấu hình clock 72 MHz, APB1 36 MHz.
2. Bật CAN1, chọn PB8 RX/PB9 TX (remap), Normal mode, Prescaler **4**, Time Quanta in Bit Segment 1 **15 TQ**, Segment 2 **2 TQ**, SJW **1 TQ**. Với PCLK1 36 MHz: `36 MHz / (4 × (1 + 15 + 2)) = 500 kbit/s`. Bật ngắt `USB low priority or CAN RX0` trong NVIC. Nếu APB1 không phải 36 MHz, tính lại prescaler.
3. Copy `stm32/can_echo.c` và `stm32/can_echo.h` vào `Core/Src` và `Core/Inc`; thêm `can_echo.c` vào build nếu IDE chưa tự nhận.
4. Trong `main.c`, sau `MX_CAN_Init()` gọi `if (CAN_Echo_Start() != HAL_OK) Error_Handler();`. Trong `while (1)` gọi `CAN_Echo_Process();` thường xuyên. Giữ hàm `USB_LP_CAN1_RX0_IRQHandler()` do CubeMX tạo và bảo đảm nó gọi `HAL_CAN_IRQHandler(&hcan)`.
5. Build và flash STM32. Nếu code đã có callback `HAL_CAN_RxFifo0MsgPendingCallback`, gộp logic callback thay vì khai báo hai lần.

## Kịch bản kiểm thử

| Bước | Thao tác | Kết quả mong đợi |
| --- | --- | --- |
| 1 | Tắt nguồn, kiểm tra CANH/CANL và đo điện trở giữa hai dây | Khoảng 60 Ω với hai điện trở 120 Ω |
| 2 | Cấp nguồn, đo VCC MCP2551 và RXD trước khi nối MCU | VCC khoảng 5 V; đầu vào RX tại MCU không quá 3,3 V |
| 3 | Nạp STM32 rồi ESP, mở monitor ESP | `PASS seq=...` tăng mỗi giây, không có `TIMEOUT` |
| 4 | Tháo kết nối CANH hoặc tắt STM32 | ESP báo lỗi TX, `TIMEOUT` hoặc bus-off; không bị treo vĩnh viễn |
| 5 | Nối lại bus/khởi động lại hai board | `PASS` xuất hiện lại; nếu bus-off kéo dài, reset ESP |

Nếu TX lỗi: kiểm tra node thứ hai đã bật để ACK, bitrate, CANH/CANL và mass chung. Nếu TX thành công nhưng `TIMEOUT`: kiểm tra ngắt RX0, filter ID 0x100, remap PB8/PB9 và dây TXD/RXD. Nếu lỗi xuất hiện ngẫu nhiên: kiểm tra termination, nguồn 5 V và độ dài dây.

## Tài liệu gốc

- [Espressif ESP32-S3 TWAI](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/twai.html)
- [ST RM0008, CAN1 pin remap và bxCAN](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-microcontrollers-stmicroelectronics.pdf)
- [Microchip MCP2551 product/datasheet](https://www.microchip.com/en-us/product/MCP2551)
