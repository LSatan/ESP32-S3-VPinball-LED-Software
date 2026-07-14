using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace DirectOutput.Cab.Out.AdressableLedStrip
{
    /// <summary>
    /// ESP32-S3 16-Channel & Bulk Stream Extension Controller.
    /// Derived from WemosD1MPStripController to maintain backward compatibility.
    /// 
    /// New Features:
    /// - Enable16ChannelMode: Unlocks channels 11 through 16.
    /// - EnableBulkMode: Replaces the per-strip ping-pong handshake with a single, highly efficient stream-based transfer ('W' command).
    /// 
    /// For the compatible ESP32-S3 firmware, visit: <a target="_blank" href="https://github.com/LSatan/ESP32-S3-VPinball-LED-Software">LSatan's ESP32-S3 VPinball LED Software</a>
    /// </summary>
    public class Esp32S3StripController : WemosD1MPStripController
    {
        public Esp32S3StripController()
        {
            // Expand the underlying array to support up to 16 channels for ESP32-S3
            NumberOfLedsPerStrip = new int[16];
        }

        private bool _Enable16ChannelMode = false;

        /// <summary>
        /// Unlocks channels 11 to 16 for ESP32-S3 based controllers.
        /// </summary>
        public bool Enable16ChannelMode
        {
            get { return _Enable16ChannelMode; }
            set { _Enable16ChannelMode = value; }
        }

        private bool _EnableBulkMode = false;

        /// <summary>
        /// Uses the highly efficient 'W' bulk transfer command instead of sequential ping-pong.
        /// </summary>
        public bool EnableBulkMode
        {
            get { return _EnableBulkMode; }
            set { _EnableBulkMode = value; }
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

        protected override void SetupController()
        {
            if (!Enable16ChannelMode && !EnableBulkMode)
            {
                // Fall back to standard Wemos setup if no ESP32 specific features are enabled
                base.SetupController();
                return;
            }

            byte[] ReceiveData = null;
            int BytesRead = -1;
            byte[] CommandData = null;

            int maxChannels = Enable16ChannelMode ? 16 : 10;
            int activeStrips = Math.Min(NumberOfLedsPerStrip.Length, maxChannels);

            // Set firm hardware configuration timeouts slightly higher for safe initial handwriting
            ComPort.ReadTimeout = 150;
            ComPort.WriteTimeout = 150;

            if (SendPerLedstripLength || EnableBulkMode)
            {
                for (var numled = 0; numled < activeStrips; ++numled)
                {
                    int nbleds = NumberOfLedsPerStrip[numled];
                    if (nbleds > 0)
                    {
                        int setupRetries = 3;
                        bool setupSuccess = false;

                        while (setupRetries > 0 && !setupSuccess)
                        {
                            try
                            {
                                // Proactively purge buffers before critical init sequence commands
                                if (ComPort.BytesToRead > 0)
                                {
                                    ComPort.DiscardInBuffer();
                                }

                                CommandData = new byte[5] { (byte)'Z', (byte)numled, (byte)(activeStrips - 1), (byte)(nbleds >> 8), (byte)(nbleds & 255) };
                                Log.Write($"Resize ledstrip {numled} to {nbleds} leds. (Attempts remaining: {setupRetries})");
                                ComPort.Write(CommandData, 0, 5);

                                ReceiveData = new byte[1];
                                BytesRead = ReadPortWait(ReceiveData, 0, 1);

                                if (BytesRead == 1 && ReceiveData[0] == (byte)'A')
                                {
                                    setupSuccess = true;
                                }
                                else
                                {
                                    setupRetries--;
                                    Log.Write($"Warning: Init ACK invalid for channel {numled}. Retrying...");
                                    Thread.Sleep(50);
                                }
                            }
                            catch (TimeoutException)
                            {
                                setupRetries--;
                                Log.Write($"Warning: Timeout during setup handshake for channel {numled}. Retrying...");
                                Thread.Sleep(50);
                            }
                            catch (Exception E)
                            {
                                throw new Exception($"Fatal exception encountered during structural hardware initialization of channel {numled}.", E);
                            }
                        }

                        if (!setupSuccess)
                        {
                            throw new Exception($"Critical: The controller failed to acknowledge the configuration payload ('Z') for channel {numled} after 3 attempts.");
                        }
                    }
                }
            }

            if (TestOnConnect)
            {
                CommandData = new byte[1] { (byte)'T' };
                Log.Write("Send a test request to the controller");
                ComPort.Write(CommandData, 0, 1);
                Thread.Sleep(2000);

                ReceiveData = new byte[1];
                BytesRead = -1;
                try
                {
                    BytesRead = ReadPortWait(ReceiveData, 0, 1);
                }
                catch (Exception E)
                {
                    throw new Exception("Expected 1 bytes after requesting a test sequence.", E);
                }

                if (BytesRead != 1 || ReceiveData[0] != (byte)'A')
                {
                    throw new Exception("Expected a Ack (A) after requesting a test sequence.");
                }

                TestOnConnect = false;
            }
        }

        protected override void UpdateOutputs(byte[] OutputValues)
        {
            if (ComPort == null || !ComPort.IsOpen) return;

            if (!EnableBulkMode)
            {
                // If Bulk Mode is off, let the robust Wemos base class handle the classic ping-pong transmission 
                // This ensures we reuse the Wemos Watchdog we built previously!
                base.UpdateOutputs(OutputValues);
                return;
            }

            int maxChannels = Enable16ChannelMode ? 16 : 10;

            try
            {
                // Enforce active runtime transmission configuration
                ComPort.ReadTimeout = 70;
                ComPort.WriteTimeout = 70;

                // -----------------------------------------------------------------
                // STREAM-BASED BULK TRANSFER MODE ('W')
                // -----------------------------------------------------------------
                UncompressedData.Clear();
                int sourcePosition = 0;

                for (int i = 0; i < maxChannels; i++)
                {
                    int NrOfLedsOnStrip = NumberOfLedsPerStrip[i];
                    if (NrOfLedsOnStrip > 0)
                    {
                        UncompressedData.AddRange(OutputValues.Skip(sourcePosition * 3).Take(NrOfLedsOnStrip * 3));
                        sourcePosition += NrOfLedsOnStrip;
                    }
                }

                if (UncompressedData.Count > 0)
                {
                    byte[] dataToSend;
                    byte isCompressedFlag = 0;
                    ushort packetSize = 0;

                    if (UseCompression)
                    {
                        CompressedData.Clear();
                        List<byte> tempUncompressed = new List<byte>(UncompressedData);

                        while (tempUncompressed.Count > 0)
                        {
                            if (tempUncompressed.Count == 3)
                            {
                                CompressedData.Add(1);
                                CompressedData.Add(tempUncompressed[0]);
                                CompressedData.Add(tempUncompressed[1]);
                                CompressedData.Add(tempUncompressed[2]);
                                tempUncompressed.RemoveRange(0, 3);
                            }
                            else
                            {
                                byte r = tempUncompressed[0];
                                byte g = tempUncompressed[1];
                                byte b = tempUncompressed[2];
                                tempUncompressed.RemoveRange(0, 3);
                                int value = (r << 16) | (g << 8) | b;
                                int cnt = 1;
                                while (tempUncompressed.Count > 0 && ((tempUncompressed[0] << 16) | (tempUncompressed[1] << 8) | tempUncompressed[2]) == value && cnt < byte.MaxValue - 1)
                                {
                                    tempUncompressed.RemoveRange(0, 3);
                                    cnt++;
                                }
                                CompressedData.Add((byte)cnt);
                                CompressedData.Add(r);
                                CompressedData.Add(g);
                                CompressedData.Add(b);
                            }
                        }
                        dataToSend = CompressedData.ToArray();
                        isCompressedFlag = 1;
                        packetSize = (ushort)(CompressedData.Count / 4);
                    }
                    else
                    {
                        dataToSend = UncompressedData.ToArray();
                        isCompressedFlag = 0;
                        packetSize = (ushort)(UncompressedData.Count / 3);
                    }

                    byte[] bulkHeader = new byte[4] {
                        (byte)'W',
                        isCompressedFlag,
                        (byte)(packetSize >> 8),
                        (byte)(packetSize & 255)
                    };

                    // Clean out unexpected pre-existing hardware buffer remainders
                    try
                    {
                        if (ComPort.BytesToRead > 0)
                        {
                            ComPort.DiscardInBuffer();
                        }
                    }
                    catch { }

                    ComPort.Write(bulkHeader, 0, 4);
                    ComPort.Write(dataToSend, 0, dataToSend.Length);

                    int BytesRead = -1;
                    byte[] AnswerData = new byte[1];

                    try
                    {
                        BytesRead = ComPort.Read(AnswerData, 0, 1);

                        if (BytesRead != 1 || AnswerData[0] != (byte)'A')
                        {
                            Log.Write($"Warning: Bulk ACK invalid. Expected 'A', got {(BytesRead == 1 ? AnswerData[0].ToString() : "nothing")}. Frame skipped.");
                        }
                    }
                    catch (TimeoutException)
                    {
                        // Soft-Fail: Drop the frame silently without freezing the DOF thread
                        Log.Write("Warning: Timeout waiting for Bulk ACK. Frame lost, but continuing...");
                        try
                        {
                            ComPort.DiscardOutBuffer();
                            ComPort.DiscardInBuffer();
                        }
                        catch { }
                    }
                }
            }
            catch (Exception E)
            {
                // -----------------------------------------------------------------
                // WATCHDOG AUTOMATED HARDWARE RECOVERY LAYER (Physical Disconnection)
                // -----------------------------------------------------------------
                Log.Write($"[DOF Critical] Hardware transmission pipe crashed ({E.Message}). Starting automated hot-plug recovery...");

                bool reconnectionSuccessful = false;
                int maxAttempts = 15;
                int delayBetweenAttemptsMs = 1000;

                for (int attempt = 1; attempt <= maxAttempts; attempt++)
                {
                    try
                    {
                        Log.Write($"[DOF Watchdog Recovery] Processing loop attempt {attempt} of {maxAttempts}...");

                        try
                        {
                            if (ComPort.IsOpen)
                            {
                                ComPort.Close();
                            }
                        }
                        catch { }

                        Thread.Sleep(delayBetweenAttemptsMs);

                        ComPort.Open();

                        Log.Write("[DOF Watchdog Recovery] COM-Port handle reclaimed. Re-initializing custom ESP32 payload array structures...");

                        SetupController();

                        Log.Write("[DOF Watchdog Recovery] SUCCESS! Controller hardware array re-established. Resuming runtime render frame processing.");
                        reconnectionSuccessful = true;
                        break;
                    }
                    catch (Exception reattemptEx)
                    {
                        Log.Write($"[DOF Watchdog Recovery] Recovery branch tracking attempt {attempt} failed: {reattemptEx.Message}");
                    }
                }

                if (!reconnectionSuccessful)
                {
                    Log.Write("[DOF Critical Watchdog Failure] Recovery window expired without valid device tracking. Killing pipeline updater thread.");
                    throw new Exception("DirectOutput completely lost physical connection stability to the custom ESP32 architecture. Hardware link failure.", E);
                }
            }
        }
    }
}