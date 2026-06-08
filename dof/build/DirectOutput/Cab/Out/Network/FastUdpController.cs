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
        private int _longestStripLeds = 0; // Merkt sich die Laenge des laengsten Streifens

        // --- CHUNKING VARIABLEN ---
        private byte _frameId = 0;
        private const int MAX_PAYLOAD_SIZE = 1400;

        // --- NEU: HARDWARE-LATENZ STOPPUHR ---
        private Stopwatch _hardwareRenderWatch = new Stopwatch();
        private long _requiredRenderTicks = 0; // Dynamische Sperrzeit in CPU-Takten

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
                // Den laengsten Streifen fuer die Latenz-Berechnung finden
                if (strips[i] > _longestStripLeds) { _longestStripLeds = strips[i]; }
            }

            // Mathematisch exakte Renderzeit berechnen:
            // 30 Mikrosekunden pro LED + 300 Mikrosekunden WS2812-Reset-Latch
            double renderTimeMs = (_longestStripLeds * 0.02);

            // Umrechnen in hochpraezise CPU-Ticks
            _requiredRenderTicks = (long)(renderTimeMs * Stopwatch.Frequency / 1000.0);

            return _totalLeds * 3;
        }

        protected override bool VerifySettings() { return true; }

        protected override void ConnectToController()
        {
            // Ruft GetNumberOfConfiguredOutputs intern auf und berechnet die Ticks
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
            _hardwareRenderWatch.Start(); // Startet die globale Überwachungsuhr
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
                // --- 1. DIE HARDWARE-BREMSE (Busy-Wait VOR dem Senden) ---
                // Wenn der ESP32 rein physikalisch noch mit dem Zeichnen des 
                // vorherigen Bildes beschaeftigt ist, MUSS DOF hier warten.
                while (_hardwareRenderWatch.ElapsedTicks < _requiredRenderTicks)
                {
                    // Warten, bis die physikalische LED-Uhr abgelaufen ist
                }

                // Uhr sofort neu starten für das aktuelle Frame
                _hardwareRenderWatch.Restart();

                // 2. Das große Array in einem Rutsch fertigstellen
                Buffer.BlockCopy(OutputValues, 0, _ledBuffer, _headerSize, OutputValues.Length);

                // 3. Ausrechnen, wie viele Häppchen wir brauchen
                int totalChunks = (int)Math.Ceiling((double)_ledBuffer.Length / MAX_PAYLOAD_SIZE);

                // --- SETUP FÜR DEINE INTERNE CHUNK-BREMSE (Für LAN) ---
                Stopwatch sw = new Stopwatch();
                int burstSize = 10;
                long waitTicksBurst = (long)(4.0 * Stopwatch.Frequency / 1000.0); // 0.3 ms Pause zwischen Chunks
                long waitTicksEnd = (long)(4.0 * Stopwatch.Frequency / 1000.0); // 4.0 ms Pause am Ende bei Riesenpaketen

                // 4. Häppchen verpacken und senden
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

                    // --- DIE CHUNK-HANDBREMSE ---
                    if ((i + 1) % burstSize == 0 && (i + 1) < totalChunks)
                    {
                        sw.Restart();
                        while (sw.ElapsedTicks < waitTicksBurst) { }
                    }
                }

                _frameId++;

                // 5. DEINE ORIGINAL-ENDPAUSE (Wird wie gewuenscht auf die Renderzeit draufaddiert)
                if (totalChunks > burstSize)
                {
                    sw.Restart();
                    while (sw.ElapsedTicks < waitTicksEnd) { }
                }
            }
        }
    }
}