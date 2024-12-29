package middle.llvm;

import middle.IrType;

import java.util.ArrayList;
import java.util.stream.Collectors;

public class BasicBlock extends User {
    private Function parentFunction;
    // 基本块内的指令列表
    private final ArrayList<Instruction> instructions = new ArrayList<>();

    public BasicBlock(Function parentFunction) {
        super(IrData.getBlockName(), IrType.LabelType.LABEL);
        this.parentFunction = parentFunction;
        parentFunction.addBasicBlock(this);
    }

    public void addInstruction(Instruction instruction) {
        instructions.add(instruction);
    }

    @Override
    public String toString() {
        return getName() + ":\n\t" +
                instructions.stream().map(Object::toString)
                        .collect(Collectors.joining("\n\t"));
    }
}
