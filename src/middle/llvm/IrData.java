package middle.llvm;

public class IrData {
    private static int varCnt = 0;
    private static int blockCnt = 0;
    public static String getVarName() {
        return "%v" + varCnt++;
    }

    public static String getBlockName() {
        return "b" + blockCnt++;
    }
}
