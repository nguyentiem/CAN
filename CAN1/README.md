# CAN1: STM32F103 gửi yêu cầu

Firmware này dùng bxCAN của STM32F103C8T6 tại **PA11 (CAN_RX)** và **PA12 (CAN_TX)**. Cứ 1000 ms, nó đưa một frame dữ liệu chuẩn ID `0x200`, DLC 8, vào TX mailbox. Bộ lọc chỉ nhận frame chuẩn ID `0x201` từ project `../CAN` và ghi log frame nhận.

Hai project dùng cùng clock APB1 8 MHz và CAN 125 kbit/s: prescaler 4, BS1 13 TQ, BS2 2 TQ, SJW 1 TQ. Cấu hình chân là CAN remap case 1 (PA11/PA12).

## Đấu nối

Mỗi STM32 cần **một CAN transceiver**. Nối PA12 tới TXD và RXD tới PA11 của transceiver bên này; bên `CAN` dùng PB9 tới TXD và RXD tới PB8. Nối CANH với CANH, CANL với CANL và chung GND. Đặt điện trở 120 Ω giữa CANH/CANL ở **mỗi đầu** bus (không lắp trùng nếu module đã có). Không nối trực tiếp hai cặp GPIO của MCU hoặc nối CANH/CANL vào GPIO.

Nếu dùng MCP2551 5 V: nối RS xuống GND, cấp 5 V và tụ lọc sát IC; kiểm tra mức RXD đưa về STM32 phù hợp 3,3 V (dùng transceiver 3,3 V hoặc mạch chuyển mức phù hợp). Đo khoảng 60 Ω giữa CANH/CANL khi tắt nguồn nếu hai đầu đều có 120 Ω.

## Build và log

```sh
cmake --preset Debug
cmake --build --preset Debug
```

Log Debug qua SEGGER RTT channel 0. Dòng `TX queued` nghĩa là frame đã vào mailbox, **chưa xác nhận đã được ACK trên bus**. Khi kết nối đúng, bên này có log `TX queued id=0x00000200 ...` mỗi giây rồi `RX id=0x00000201 ...`; bên `CAN` có log RX và TX tương ứng. Build Release tắt log.

HSI nội 8 MHz thích hợp thử dây ngắn trên bàn; nếu cần độ ổn định cao hơn, dùng clock ngoài và tính lại timing CAN cho cả hai board. Chưa kiểm tra giao tiếp trên phần cứng trong môi trường này.
