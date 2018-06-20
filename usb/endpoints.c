#include "endpoints.h"
#include "CH554_SDCC.h"
#include "compiler.h"

// #include "usb_descriptor.h"
#include "descriptor.h"

// #include <stdio.h>
#include <string.h>

#define THIS_ENDP0_SIZE         DEFAULT_ENDP0_SIZE


/*键盘数据*/
static uint8_t __xdata HIDData[8] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};
/**
 * @brief 端点0缓冲区。
 *
 * 地址0x00-0x08 为端点0的IN与OUT缓冲区
 *
 */
uint8_t __xdata __at (0x00) Ep0Buffer[THIS_ENDP0_SIZE];
/**
 * @brief 端点1缓冲区，用于键盘报文
 *
 * 地址0x88-0xC7为端点1OUT缓冲区 （实际使用1byte)
 * 地址0xC8-0xCF为端点1IN缓冲区 (8byte)
 *
 */
uint8_t __xdata __at (0x0A) Ep1Buffer[MAX_PACKET_SIZE + 8];  //端点1 IN缓冲区,必须是偶地址
/**
 * @brief 端点2IN缓冲区，用于System包和Consumer包的发送
 *
 */
uint8_t __xdata __at (0x54) Ep2Buffer[2];
/**
 * @brief 端点3IN&OUT缓冲区，用于传递配置
 *
 */
uint8_t __xdata __at (0x58) Ep3Buffer[2];  //端点3 IN缓冲区,必须是偶地址

uint8_t SetupReq,SetupLen,Ready,Count,SendFinish,UsbConfig;
uint8_t *pDescr;
uint8_t len = 0;
// 键盘报文类型。0为Boot，1为Report
uint8_t keyboard_protocol = 1;
uint8_t keyboard_idle = 0;

USB_SETUP_REQ   SetupReqBuf;                                                   //暂存Setup包
#define UsbSetupBuf     ((PUSB_SETUP_REQ)Ep0Buffer)

void nop() {}

void EP0_OUT()
{
    len = USB_RX_LEN;
    switch (SetupReq)
    {
        case USB_GET_DESCRIPTOR:
            UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;           // 准备下一控制传输
            break;
    }
}
void EP0_IN()
{
    switch(SetupReq)
    {
        case USB_GET_DESCRIPTOR:
            len = SetupLen >= THIS_ENDP0_SIZE ? THIS_ENDP0_SIZE : SetupLen;                          //本次传输长度
            memcpy( Ep0Buffer, pDescr, len );                            //加载上传数据
            SetupLen -= len;
            pDescr += len;
            UEP0_T_LEN = len;
            UEP0_CTRL ^= bUEP_T_TOG;                                     //同步标志位翻转
            break;
        case USB_SET_ADDRESS:
            USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | SetupLen;
            UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
        default:
            UEP0_T_LEN = 0;                                              //状态阶段完成中断或者是强制上传0长度数据包结束控制传输
            UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
    }
}
void EP0_SETUP()
{
    len = USB_RX_LEN;
    if(len == (sizeof(USB_SETUP_REQ)))
    {
        SetupLen = UsbSetupBuf->wLengthL;
        if(UsbSetupBuf->wLengthH || SetupLen > 0x7F )
        {
            SetupLen = 0x7F;    // 限制总长度
        }
        len = 0;                                                        // 默认为成功并且上传0长度
        SetupReq = UsbSetupBuf->bRequest;
        switch(UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK)
        {
            case USB_REQ_TYP_STANDARD: //标准请求
            {
                switch(SetupReq) //请求码
                {
                    case USB_GET_DESCRIPTOR:
                        Ready = GetUsbDescriptor(UsbSetupBuf->wValueH, UsbSetupBuf->wValueL, UsbSetupBuf->wIndexL, &len, &pDescr);
                        // printf_tiny("GetDesc(%x, %x, %x)\n", UsbSetupBuf->wValueH, UsbSetupBuf->wValueL, UsbSetupBuf->wIndexL);
                        if ( SetupLen > len ) SetupLen = len;    //限制总长度
                        len = SetupLen >= THIS_ENDP0_SIZE ? THIS_ENDP0_SIZE : SetupLen;                  //本次传输长度
                        memcpy(Ep0Buffer,pDescr,len);                        //加载上传数据
                        SetupLen -= len;
                        pDescr += len;
                        break;

                    case USB_SET_ADDRESS:
                        SetupLen = UsbSetupBuf->wValueL;                     //暂存USB设备地址
                        break;

                    case USB_GET_CONFIGURATION:
                        Ep0Buffer[0] = UsbConfig;
                        if ( SetupLen >= 1 ) len = 1;
                        break;

                    case USB_SET_CONFIGURATION:
                        UsbConfig = UsbSetupBuf->wValueL;
                        break;

                    case USB_GET_INTERFACE:
                        Ep0Buffer[0] = 0x00;
                        if ( SetupLen >= 1 ) len = 1;
                        break;

                    case USB_CLEAR_FEATURE:
                    {
                        switch (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK)
                        {
                            case USB_REQ_RECIP_ENDP:
                            {
                                switch (UsbSetupBuf->wIndexL)
                                {
                                case 0x83:
                                    UEP3_CTRL = UEP3_CTRL & ~ ( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
                                    break;
                                case 0x82:
                                    UEP2_CTRL = UEP2_CTRL & ~ ( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
                                    break;
                                case 0x81:
                                    UEP1_CTRL = UEP1_CTRL & ~ ( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
                                    break;
                                case 0x03:
                                    UEP3_CTRL = UEP3_CTRL & ~ ( bUEP_R_TOG | MASK_UEP_R_RES ) | UEP_R_RES_ACK;
                                    break;
                                case 0x02:
                                    UEP2_CTRL = UEP2_CTRL & ~ ( bUEP_R_TOG | MASK_UEP_R_RES ) | UEP_R_RES_ACK;
                                    break;
                                case 0x01:
                                    UEP1_CTRL = UEP1_CTRL & ~ ( bUEP_R_TOG | MASK_UEP_R_RES ) | UEP_R_RES_ACK;
                                    break;
                                default:
                                    len = 0xFF;                                            // 不支持的端点
                                    break;
                                }
                                break;
                            }
                            case USB_REQ_RECIP_DEVICE:
                                break;
                            default: //unsupport
                                len=0xff;
                                break;
                        }
                        break;
                    }
                    case USB_SET_FEATURE:                                              /* Set Feature */
                    {
                        switch (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK)
                        {
                            case USB_REQ_RECIP_ENDP:
                            {

                                if((((uint16_t)UsbSetupBuf->wValueH << 8 ) | UsbSetupBuf->wValueL ) == 0x00 )
                                {
                                    // Zero, Interface endpoint
                                    switch( ( ( uint16_t )UsbSetupBuf->wIndexH << 8 ) | UsbSetupBuf->wIndexL )
                                    {
                                        case 0x83:
                                            UEP3_CTRL = UEP3_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;/* 设置端点2 IN STALL */
                                            break;
                                        case 0x82:
                                            UEP2_CTRL = UEP2_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;/* 设置端点2 IN STALL */
                                            break;
                                        case 0x81:
                                            UEP1_CTRL = UEP1_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;/* 设置端点1 IN STALL */
                                            break;
                                        default:
                                            len = 0xFF;                               //操作失败
                                            break;
                                    }
                                }
                                else
                                {
                                    len = 0xFF;                                   //操作失败
                                }
                                break;
                            }
                            case USB_REQ_RECIP_DEVICE:
                            {
                                if( ( ( ( uint16_t )UsbSetupBuf->wValueH << 8 ) | UsbSetupBuf->wValueL ) == 0x01 )
                                {
                                    /*
                                    if( USB_SUPPORT_REM_WAKE & 0x20 )
                                    {
                                        // 设置唤醒使能标志
                                    }
                                    else
                                    {
                                        len = 0xFF;                                        // 操作失败
                                    }
                                    */
                                }
                                else
                                {
                                    len = 0xFF;                                            /* 操作失败 */
                                }
                                break;
                            }
                            default:
                                len=0xff;
                                break;
                        }
                        break;
                    }
                    case USB_GET_STATUS:
                        Ep0Buffer[0] = 0x00;
                        Ep0Buffer[1] = 0x00;
                        len = SetupLen > 2 ? 2 : SetupLen;
                        break;
                    default:
                        len = 0xff;                                           //操作失败
                        break;
                }
                break;
            }
            case USB_REQ_TYP_CLASS: //HID类请求
            {
                switch( SetupReq )
                {
                    case 0x01://GetReport
                        if(UsbSetupBuf->wIndexL == 0 && (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                        {
                            len = 8;
                            memcpy(Ep0Buffer, &Ep1Buffer[64], 8);
                        }
                        break;
                    case 0x02://GetIdle
                        if(UsbSetupBuf->wIndexL == 0 && (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                        {
                            Ep0Buffer[0] = keyboard_idle;
                            len = 1;
                        }
                        break;
                    case 0x03://GetProtocol
                        if(UsbSetupBuf->wIndexL == 0 && (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                        {
                            Ep0Buffer[0] = keyboard_protocol;
                            len = 1;
                        }
                        break;
                    case 0x09://SetReport
                        if(UsbSetupBuf->wIndexL == 0 && (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                        {
                            Ep1Buffer[0] = Ep0Buffer[0];
                            EP1_OUT();
                        }
                        break;
                    case 0x0A://SetIdle
                        if(UsbSetupBuf->wIndexL == 0 && (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                        {
                            keyboard_idle = UsbSetupBuf->wValueH;
                        }
                        break;
                    case 0x0B://SetProtocol
                        if(UsbSetupBuf->wIndexL == 0 && (UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                        {
                            keyboard_protocol = UsbSetupBuf->wValueL;
                        }
                        break;
                    default:
                        len = 0xFF;/*命令不支持*/
                        break;
                }
                break;
            }
            case USB_REQ_TYP_VENDOR:
                break;
            case USB_REQ_TYP_RESERVED:
            default:
                break;
        }
    }
    else
    {
        len = 0xff;                                                   //包长度错误
    }
    if(len == 0xff)
    {
        SetupReq = 0xFF;
        UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;//STALL
    }
    else if(len <= DEFAULT_ENDP0_SIZE)                                                //上传数据或者状态阶段返回0长度包
    {
        UEP0_T_LEN = len;
        UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;//默认数据包是DATA1，返回应答ACK
    }
    else
    {
        UEP0_T_LEN = 0;  //虽然尚未到状态阶段，但是提前预置上传0长度数据包以防主机提前进入状态阶段
        UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;//默认数据包是DATA1,返回应答ACK
    }
}

void EP1_IN()
{
    // printf_tiny("EP1_IN\n");
    UEP1_T_LEN = 0;                                                     //预使用发送长度一定要清空
//  UEP2_CTRL ^= bUEP_T_TOG;                                            //如果不设置自动翻转则需要手动翻转
    UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;           //默认应答NAK
    SendFinish = 1;                                                           //传输完成标志
}

void EP2_IN()
{
    // printf_tiny("EP2_IN\n");
    UEP2_T_LEN = 0;                                                     //预使用发送长度一定要清空
//  UEP1_CTRL ^= bUEP_T_TOG;                                            //如果不设置自动翻转则需要手动翻转
    UEP2_CTRL = UEP2_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;           //默认应答NAK
}

void EP3_IN()
{
    UEP3_T_LEN = 0;                                                     //预使用发送长度一定要清空
    UEP3_CTRL = UEP3_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;           //默认应答NAK
}



/** \brief USB设备模式配置,设备模式启动，收发端点配置，中断开启
 *
 */
void USBDeviceInit()
{
    IE_USB = 0;
    USB_CTRL = 0x00;                                                           // 先设定USB设备模式

    UEP0_DMA = (uint16_t)Ep0Buffer;                                            //端点0数据传输地址
    UEP4_1_MOD &= ~(bUEP4_RX_EN | bUEP4_TX_EN);                                   //端点0单64字节收发缓冲区, 端点4单64字节收发缓冲区
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;                                 //OUT事务返回ACK，IN事务返回NAK

    UEP1_DMA = (uint16_t)Ep1Buffer;                                            //端点1数据传输地址
    UEP4_1_MOD = UEP4_1_MOD & ~bUEP1_BUF_MOD | bUEP1_TX_EN | bUEP1_RX_EN;      //端点1收发使能 64字节收发缓冲区
    UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;                                 //端点1自动翻转同步标志位，IN事务返回NAK

    UEP2_DMA = (uint16_t)Ep2Buffer;                                            //端点2数据传输地址
    UEP2_3_MOD = UEP2_3_MOD & ~bUEP2_BUF_MOD | bUEP2_TX_EN ;                   //端点2接收使能 64字节缓冲区
    UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;                                 //端点2自动翻转同步标志位，IN事务返回NAK

    UEP3_DMA = (uint16_t)Ep3Buffer;                                            //端点3数据传输地址
    UEP2_3_MOD = UEP2_3_MOD & ~bUEP3_BUF_MOD | bUEP3_TX_EN | bUEP1_RX_EN;      //端点3接收使能 64字节缓冲区
    UEP3_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;                                 //端点3自动翻转同步标志位，IN事务返回NAK

    USB_DEV_AD = 0x00;
    UDEV_CTRL = bUD_PD_DIS;                                                    // 禁止DP/DM下拉电阻
    USB_CTRL = bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;                      // 启动USB设备及DMA，在中断期间中断标志未清除前自动返回NAK
    UDEV_CTRL |= bUD_PORT_EN;                                                  // 允许USB端口
    USB_INT_FG = 0xFF;                                                         // 清中断标志
    USB_INT_EN = bUIE_SUSPEND | bUIE_TRANSFER | bUIE_BUS_RST;
    IE_USB = 1;
}
