using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace DirectOutput.Cab.Out.AdressableLedStrip
{
    /// <summary>
    /// ESP32-S3 16-Channel Extension:
    /// This controller class has been expanded to support ESP32-S3 boards, which can drive up to 16 independent LED strips.
    /// By default, it operates in a strict 10-channel mode to ensure 100% backward compatibility with standard Wemos hardware.
    /// Setting <c>Enable16ChannelMode</c> to true unlocks channels 11 through 16.
    /// 
    /// For the compatible ESP32-S3 firmware, visit: <a target="_blank" href="https://github.com/LSatan/ESP32-S3-VPinball-LED-Software">LSatan's ESP32-S3 VPinball LED Software</a>
    /// 
    /// ---------------------------------------------------------------------------------------------------------
    /// ORIGINAL WEMOS D1 MINI PRO IMPLEMENTATION:
    /// 
    /// The WemosD1MPStripController is a Teensy equivalent board called Wemos D1 Mini Pro (also known as ESP8266), it's cheaper than the Teensy and can support the same amount of Ledstrip and leds per strip.
    /// 
    /// \image html wemos-d1-mini-pro.jpg
    /// 
    /// The Wemos D1 Mini Pro firmware made by aetios50, peskopat and yoyofr can be found there <a target="_blank" href="https://github.com/aetios50/PincabLedStrip">aetios50 Github page</a>
    /// 
    /// You can also find a tutorial (in french for now) explaining the flashing process for the Wemos D1 Mini Pro using these firmware <a target="_blank" href="https://shop.arnoz.com/laboratoire/2019/10/29/flasher-unewemos-d1-mini-pro-pour-lutiliser-dans-son-pincab/">Arnoz' lab Wemos D1 flashing tutorial</a>
    /// 
    /// There is a great online tool to setup easily both Teensy and Wemos D1 based ledstrips (an english version is also available) <a target="_blank" href="https://shop.arnoz.com/laboratoire/2020/09/17/cacabinet-generator/">Arnoz' cacabinet generator</a>
    /// </summary>
    public class WemosD1MPStripController : TeensyStripController
    {
        // --- CONSTRUCTOR ---
        // Expands the array of the base class (Teensy) from 10 to 16 channels
        public WemosD1MPStripController()
        {
            NumberOfLedsPerStrip = new int[16];
        }

        private bool _Enable16ChannelMode = false;

        /// <summary>
        /// Enables 16 channel output for ESP32-S3 controllers. Defaults to false (limits to 10 channels for standard Wemos hardware).
        /// </summary>
        public bool Enable16ChannelMode
        {
            get { return _Enable16ChannelMode; }
            set { _Enable16ChannelMode = value; }
        }

        // --- NEW CHANNELS 11 TO 16 ---

        public int NumberOfLedsStrip11
        {
            get { return NumberOfLedsPerStrip[10]; }
            set { NumberOfLedsPerStrip[10] = value; base.SetupOutputs(); }
        }

        public int NumberOfLedsStrip12
        {
            get { return NumberOfLedsPerStrip[11]; }
            set { NumberOfLedsPerStrip[11] = value; base.SetupOutputs(); }
        }

        public int NumberOfLedsStrip13
        {
            get { return NumberOfLedsPerStrip[12]; }
            set { NumberOfLedsPerStrip[12] = value; base.SetupOutputs(); }
        }

        public int NumberOfLedsStrip14
        {
            get { return NumberOfLedsPerStrip[13]; }
            set { NumberOfLedsPerStrip[13] = value; base.SetupOutputs(); }
        }

        public int NumberOfLedsStrip15
        {
            get { return NumberOfLedsPerStrip[14]; }
            set { NumberOfLedsPerStrip[14] = value; base.SetupOutputs(); }
        }

        public int NumberOfLedsStrip16
        {
            get { return NumberOfLedsPerStrip[15]; }
            set { NumberOfLedsPerStrip[15] = value; base.SetupOutputs(); }
        }

        private bool _SendPerLedstripLength = false;

        public bool SendPerLedstripLength
        {
            get { return _SendPerLedstripLength; }
            set { _SendPerLedstripLength = value; }
        }

        private bool _UseCompression = false;

        public bool UseCompression
        {
            get { return _UseCompression; }
            set { _UseCompression = value; }
        }

        private bool _TestOnConnect = false;

        public bool TestOnConnect
        {
            get { return _TestOnConnect; }
            set { _TestOnConnect = value; }
        }

        protected override void SetupController()
        {
            byte[] ReceiveData = null;
            int BytesRead = -1;
            byte[] CommandData = null;

            base.SetupController();

            // Determine maximum channels based on selected hardware mode
            int maxChannels = Enable16ChannelMode ? 16 : 10;
            int activeStrips = Math.Min(NumberOfLedsPerStrip.Length, maxChannels);

            // Send number of leds per leds strips 
            if (SendPerLedstripLength)
            {
                for (var numled = 0; numled < activeStrips; ++numled)
                {
                    int nbleds = NumberOfLedsPerStrip[numled];
                    if (nbleds > 0)
                    {
                        CommandData = new byte[5] { (byte)'Z', (byte)numled, (byte)(activeStrips - 1), (byte)(nbleds >> 8), (byte)(nbleds & 255) };
                        Log.Write($"Resize ledstrip {numled} to {nbleds} leds.");
                        ComPort.Write(CommandData, 0, 5);
                        ReceiveData = new byte[1];
                        BytesRead = -1;
                        try
                        {
                            BytesRead = ReadPortWait(ReceiveData, 0, 1);
                        }
                        catch (Exception E)
                        {
                            throw new Exception($"Expected 1 bytes after setting the number of leds for ledstrip {numled} , but the read operation resulted in a exception. Will not send data to the controller.", E);
                        }

                        if (BytesRead != 1 || ReceiveData[0] != (byte)'A')
                        {
                            throw new Exception($"Expected a Ack (A) after setting the number of leds for ledstrip {numled}, but received no answer or a unexpected answer ({(char)ReceiveData[0]}). Will not send data to the controller.");
                        }
                    }
                }
            }

            if (TestOnConnect)
            {
                CommandData = new byte[1] { (byte)'T' };
                Log.Write($"Send a test request to the controller");
                ComPort.Write(CommandData, 0, 1);

                // Temporary wait before asking the Ack
                Thread.Sleep(2000);

                ReceiveData = new byte[1];
                BytesRead = -1;
                try
                {
                    BytesRead = ReadPortWait(ReceiveData, 0, 1);
                }
                catch (Exception E)
                {
                    throw new Exception($"Expected 1 bytes after requesting a test sequence, but the read operation resulted in a exception. Will not send data to the controller.", E);
                }

                if (BytesRead != 1 || ReceiveData[0] != (byte)'A')
                {
                    throw new Exception($"Expected a Ack (A) after requesting a test sequence, but received no answer or a unexpected answer ({(char)ReceiveData[0]}). Will not send data to the controller.");
                }

                TestOnConnect = false;
            }
        }

        // --- OVERRIDE OF THE DATA OUTPUT LOOP ---
        protected override void UpdateOutputs(byte[] OutputValues)
        {
            if (ComPort == null)
            {
                throw new Exception("Comport is not initialized");
            }
            byte[] CommandData;
            byte[] AnswerData;
            int BytesRead;
            int SourcePosition = 0;

            // Dynamic limitation based on the selected mode
            int maxChannels = Enable16ChannelMode ? 16 : 10;

            for (int i = 0; i < maxChannels; i++)
            {
                int NrOfLedsOnStrip = NumberOfLedsPerStrip[i];
                if (NrOfLedsOnStrip > 0)
                {
                    int TargetPosition = i * NumberOfLedsPerChannel;

                    SendLedstripData(OutputValues.Skip(SourcePosition * 3).Take(NrOfLedsOnStrip * 3).ToArray(), TargetPosition);

                    BytesRead = -1;
                    AnswerData = new byte[1];

                    try
                    {
                        BytesRead = ComPort.Read(AnswerData, 0, 1);
                    }
                    catch (Exception E)
                    {
                        throw new Exception($"A exception occurred while waiting for the ACK after sending the data for channel {i + 1} of the {this.GetType().ToString()}.", E);
                    }
                    if (BytesRead != 1 || AnswerData[0] != (byte)'A')
                    {
                        throw new Exception($"Received no answer or a unexpected answer while waiting for the ACK after sending the data for channel {i + 1} of the {this.GetType().ToString()}.");
                    }
                    SourcePosition += NrOfLedsOnStrip;
                }
            }

            CommandData = new byte[1] { (byte)'O' };
            ComPort.Write(CommandData, 0, 1);

            BytesRead = -1;
            AnswerData = new byte[1];

            try
            {
                BytesRead = ComPort.Read(AnswerData, 0, 1);
            }
            catch (Exception E)
            {
                throw new Exception($"A exception occurred while waiting for the ACK after sending the output command (O) to the {this.GetType().ToString()}", E);
            }
            if (BytesRead != 1 || AnswerData[0] != (byte)'A')
            {
                throw new Exception($"Received no answer or a unexpected answer while waiting for the ACK after sending the output command (O) to the {this.GetType().ToString()}");
            }
        }

        protected List<byte> CompressedData = new List<byte>();
        protected List<byte> UncompressedData = new List<byte>();

        protected override void SendLedstripData(byte[] OutputValues, int TargetPosition)
        {
            if (UseCompression)
            {
                // Try a simple color based RLE compression
                CompressedData.Clear();
                UncompressedData.Clear();
                UncompressedData.AddRange(OutputValues);

                while (UncompressedData.Count > 0)
                {
                    if (UncompressedData.Count == 3)
                    {
                        CompressedData.Add(1);
                        CompressedData.Add(UncompressedData[0]);
                        CompressedData.Add(UncompressedData[1]);
                        CompressedData.Add(UncompressedData[2]);
                        UncompressedData.RemoveRange(0, 3);
                    }
                    else
                    {
                        byte r = UncompressedData[0];
                        byte g = UncompressedData[1];
                        byte b = UncompressedData[2];
                        UncompressedData.RemoveRange(0, 3);
                        int value = (r << 16) | (g << 8) | b;
                        int cnt = 1;
                        while (UncompressedData.Count > 0 && ((UncompressedData[0] << 16) | (UncompressedData[1] << 8) | UncompressedData[2]) == value && cnt < byte.MaxValue - 1)
                        {
                            UncompressedData.RemoveRange(0, 3);
                            cnt++;
                        }
                        CompressedData.Add((byte)cnt);
                        CompressedData.Add(r);
                        CompressedData.Add(g);
                        CompressedData.Add(b);
                    }
                }

                if (CompressedData.Count < OutputValues.Length)
                {
                    var nbData = CompressedData.Count / 4;
                    var nbLeds = OutputValues.Length / 3;
                    byte[] CommandData = new byte[7] {  (byte)'Q',
                                                    (byte)(TargetPosition >> 8), (byte)(TargetPosition & 255),
                                                    (byte)(nbData >> 8), (byte)(nbData & 255),
                                                    (byte)(nbLeds >> 8), (byte)(nbLeds & 255)
                                                    };
                    ComPort.Write(CommandData, 0, 7);
                    ComPort.Write(CompressedData.ToArray(), 0, CompressedData.Count);
                }
                else
                {
                    base.SendLedstripData(OutputValues, TargetPosition);
                }
            }
            else
            {
                base.SendLedstripData(OutputValues, TargetPosition);
            }
        }
    }
}