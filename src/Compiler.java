import antlr.SysY2022Lexer;
import antlr.SysY2022Parser;
import middle.IrVisitor;
import middle.llvm.IrModule;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.CommonTokenStream;
import org.antlr.v4.runtime.tree.ParseTree;

public class Compiler {
    public static void main(String[] args) throws Exception {
        CharStream inputStream = CharStreams.fromFileName("testfile.sy");
        SysY2022Lexer lexer = new SysY2022Lexer(inputStream);
        CommonTokenStream tokens = new CommonTokenStream(lexer);
        SysY2022Parser parser = new SysY2022Parser(tokens);
        ParseTree tree = parser.compUnit();
        IrVisitor visitor = new IrVisitor();
        visitor.visit(tree);
        System.out.println(IrModule.getInstance());
    }
}
