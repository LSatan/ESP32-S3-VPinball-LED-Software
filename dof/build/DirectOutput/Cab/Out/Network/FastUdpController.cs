using System;
using System.Net.Sockets;
using System.Diagnostics;
using DirectOutput;
using DirectOutput.Cab.Out;

namespace DirectOutput.Cab.Out.Network
{
    public class FastUdpController : OutputControllerCompleteBase
    {
        public string IpAddress { get; set; } = "192.168.4.1";
        public int Port { get; set; } = 6454;

        // --- DOF XML PROPERTIES ---
        public int NumberOfLedsStrip1 { get; set; } = 0;
        public int NumberOfLedsStrip2 { get; set; } = 0;
        public int NumberOfLedsStrip3 { get; set; } = 0;
        public int NumberOfLedsStrip4 { get; set; } = 0;
        public int NumberOfLedsStrip5 { get; set; } = 0;
        public int NumberOfLedsStrip6 { get; set; } = 0;
        public int NumberOfLedsStrip7 { get; set; } = 0;
        public int NumberOfLedsStrip8 { get; set; } = 0;
        public int NumberOfLedsStrip9 { get; set; } = 0;
        public int NumberOfLedsStrip10 { get; set; } = 0;
        public int NumberOfLedsStrip11 { get; set; } = 0;
        public int NumberOfLedsStrip12 { get; set; } = 0;
        public int NumberOfLedsStrip13 { get; set; } = 0;
        public int NumberOfLedsStrip14 { get; set; } = 0;
        public int NumberOfLedsStrip15 { get; set; } = 0;
        public int NumberOfLedsStrip16 { get; set; } = 0;

        private UdpClient _udpClient;
        private byte[] _ledBuffer;
        private int _headerSize = 33;
        private int _maxStrips = 16;
        private int _totalLeds = 0;
        private int _longestStripLeds = 0; // Remembers the length of the longest strip

        // --- CHUNKING VARIABLES ---
        private byte _frameId = 0;
        private const int MAX_PAYLOAD_SIZE = 1400;

        // --- HARDWARE LATENCY STOPWATCH ---
        private Stopwatch _hardwareRenderWatch = new Stopwatch();
        private long _requiredRenderTicks = 0; // Dynamic lock time in CPU cycles

        protected override int GetNumberOfConfiguredOutputs()
        {
            int[] strips = new int[] {
                NumberOfLedsStrip1, NumberOfLedsStrip2, NumberOfLedsStrip3, NumberOfLedsStrip4,
                NumberOfLedsStrip5, NumberOfLedsStrip6, NumberOfLedsStrip7, NumberOfLedsStrip8,
                NumberOfLedsStrip9, NumberOfLedsStrip10, NumberOfLedsStrip11, NumberOfLedsStrip12,
                NumberOfLedsStrip13, NumberOfLedsStrip14, NumberOfLedsStrip15, NumberOfLedsStrip16
            };

            _totalLeds = 0;
            _longestStripLeds = 0;

            for (int i = 0; i < _maxStrips; i++)
            {
                _totalLeds += strips[i];
                // Find the longest stripe for the latency calculation
                if (strips[i] > _longestStripLeds) { _longestStripLeds = strips[i]; }
            }

			// Calculate mathematically exact render time:
			// 30 microseconds per LED microseconds WS2812 reset latch. (tuned to 0.2)
            double renderTimeMs = (_longestStripLeds * 0.02);

            // Convert to high-precision CPU ticks
            _requiredRenderTicks = (long)(renderTimeMs * Stopwatch.Frequency / 1000.0);

            return _totalLeds * 3;
        }

        protected override bool VerifySettings() { return true; }

        protected override void ConnectToController()
        {
            // Calls GetNumberOfConfiguredOutputs internally and calculates the ticks
            GetNumberOfConfiguredOutputs();

            int[] strips = new int[] {
                NumberOfLedsStrip1, NumberOfLedsStrip2, NumberOfLedsStrip3, NumberOfLedsStrip4,
                NumberOfLedsStrip5, NumberOfLedsStrip6, NumberOfLedsStrip7, NumberOfLedsStrip8,
                NumberOfLedsStrip9, NumberOfLedsStrip10, NumberOfLedsStrip11, NumberOfLedsStrip12,
                NumberOfLedsStrip13, NumberOfLedsStrip14, NumberOfLedsStrip15, NumberOfLedsStrip16
            };

            _ledBuffer = new byte[_headerSize + (_totalLeds * 3)];
            _ledBuffer[0] = (byte)_maxStrips;

            for (int i = 0; i < _maxStrips; i++)
            {
                _ledBuffer[1 + (i * 2)] = (byte)(strips[i] >> 8);
                _ledBuffer[2 + (i * 2)] = (byte)(strips[i] & 0xFF);
            }

            _udpClient = new UdpClient();
            _hardwareRenderWatch.Start(); // Starts the global surveillance clock
        }

        protected override void DisconnectFromController()
        {
            _udpClient?.Close();
            _udpClient = null;
        }

        protected override void UpdateOutputs(byte[] OutputValues)
        {
            if (OutputValues.Length == _totalLeds * 3)
            {
				// --- 1. THE HARDWARE BRAKE (Busy-Wait BEFORE Sending) ---
				// If the ESP32 is physically still busy with drawing the
				// previous image, DOF MUST wait here.
                while (_hardwareRenderWatch.ElapsedTicks < _requiredRenderTicks)
                {
                    // Wait until the physical LED clock has run out
                }

                // Restart clock immediately for the current frame
                _hardwareRenderWatch.Restart();

                // 2. Complete the large array in one go
                Buffer.BlockCopy(OutputValues, 0, _ledBuffer, _headerSize, OutputValues.Length);

                // 3. Calculate how many snacks we need
                int totalChunks = (int)Math.Ceiling((double)_ledBuffer.Length / MAX_PAYLOAD_SIZE);

                // --- SETUP FOR INTERNAL CHUNK BRAKE (For LAN) ---
                Stopwatch sw = new Stopwatch();
                int burstSize = 10;
                long waitTicksBurst = (long)(4.0 * Stopwatch.Frequency / 1000.0); // 0.3 ms Pause zwischen Chunks
                long waitTicksEnd = (long)(4.0 * Stopwatch.Frequency / 1000.0); // 4.0 ms Pause am Ende bei Riesenpaketen

                // 4. Pack and send snacks
                for (int i = 0; i < totalChunks; i++)
                {
                    int offset = i * MAX_PAYLOAD_SIZE;
                    int payloadSize = Math.Min(MAX_PAYLOAD_SIZE, _ledBuffer.Length - offset);

                    byte[] chunk = new byte[3 + payloadSize];
                    chunk[0] = _frameId;
                    chunk[1] = (byte)i;
                    chunk[2] = (byte)totalChunks;

                    Buffer.BlockCopy(_ledBuffer, offset, chunk, 3, payloadSize);

                    _udpClient.Send(chunk, chunk.Length, IpAddress, Port);

                    // --- THE CHUNK HANDBRAKE ---
                    if ((i + 1) % burstSize == 0 && (i + 1) < totalChunks)
                    {
                        sw.Restart();
                        while (sw.ElapsedTicks < waitTicksBurst) { }
                    }
                }

                _frameId++;

                // 5. Pause after more than 10 chunks. (W5500 fix large packets)
                if (totalChunks > burstSize)
                {
                    sw.Restart();
                    while (sw.ElapsedTicks < waitTicksEnd) { }
                }
            }
        }
    }
}