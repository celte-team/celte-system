public static class Serialization {
    public static byte[] OpcodeToByteArray(int message, bool isLittleEndian = false)
    {
        byte[] bytes = new byte[4];
        for (int i = 0; i < 4; i++)
        {
            bytes[i] = (byte) ((message >> (i * 8)) & 0xFF);
        }
        if (isLittleEndian)
        {
            Array.Reverse(bytes);
        }
        return bytes;
    }

    public static int ByteArrayToOpcode(byte[] bytes, bool isLittleEndian = false)
    {
        if (isLittleEndian)
        {
            Array.Reverse(bytes);
        }
        int opcode = 0;
        for (int i = 0; i < 4; i++)
        {
            opcode |= bytes[i] << (i * 8);
        }
        return opcode;
    }
}
