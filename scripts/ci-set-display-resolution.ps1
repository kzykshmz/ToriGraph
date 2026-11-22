# Sets the CI desktop to 1920x1080 (or the largest >=1600x900 mode) so that the
# GUI layout checks in scidavis_ole_test see a realistic screen.
# Used by .github/workflows/main.yml and release-windows.yml.
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class DisplayHelper {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct DEVMODE {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
        public short dmSpecVersion; public short dmDriverVersion; public short dmSize; public short dmDriverExtra;
        public int dmFields; public int dmPositionX; public int dmPositionY; public int dmDisplayOrientation;
        public int dmDisplayFixedOutput; public short dmColor; public short dmDuplex; public short dmYResolution;
        public short dmTTOption; public short dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
        public short dmLogPixels; public int dmBitsPerPel; public int dmPelsWidth; public int dmPelsHeight;
        public int dmDisplayFlags; public int dmDisplayFrequency;
        public int dmICMMethod; public int dmICMIntent; public int dmMediaType; public int dmDitherType;
        public int dmReserved1; public int dmReserved2; public int dmPanningWidth; public int dmPanningHeight;
    }
    [DllImport("user32.dll", CharSet = CharSet.Ansi)]
    public static extern bool EnumDisplaySettings(string lpszDeviceName, int iModeNum, ref DEVMODE lpDevMode);
    [DllImport("user32.dll", CharSet = CharSet.Ansi)]
    public static extern int ChangeDisplaySettings(ref DEVMODE lpDevMode, int dwFlags);
    public static string SetBestResolution() {
        DEVMODE cur = new DEVMODE(); cur.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
        EnumDisplaySettings(null, -1, ref cur);
        string before = cur.dmPelsWidth + "x" + cur.dmPelsHeight;
        DEVMODE best = cur;
        int bestScore = cur.dmPelsWidth * cur.dmPelsHeight;
        DEVMODE dm = new DEVMODE(); dm.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
        for (int i = 0; EnumDisplaySettings(null, i, ref dm); i++) {
            if (dm.dmPelsWidth == 1920 && dm.dmPelsHeight == 1080) { best = dm; break; }
            int score = dm.dmPelsWidth * dm.dmPelsHeight;
            if (dm.dmPelsWidth >= 1600 && dm.dmPelsHeight >= 900 && score > bestScore) {
                best = dm; bestScore = score;
            }
        }
        int rc = ChangeDisplaySettings(ref best, 0);
        DEVMODE after = new DEVMODE(); after.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
        EnumDisplaySettings(null, -1, ref after);
        return "before=" + before + " -> after=" + after.dmPelsWidth + "x" + after.dmPelsHeight + " (rc=" + rc + ")";
    }
}
"@
Write-Host ([DisplayHelper]::SetBestResolution())
