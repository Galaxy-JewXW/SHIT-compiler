package middle.llvm;

import middle.IrType;

import java.util.HashSet;

public class Value {
    private final IrType type;
    private final HashSet<User> users = new HashSet<>();
    private String name;

    public Value(String name, IrType type) {
        this.name = name;
        this.type = type;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public IrType getType() {
        return type;
    }

    public HashSet<User> getUsers() {
        return users;
    }

    public void addUse(User user) {
        users.add(user);
    }

    // 删除这个值的使用者，该User不再使用此value
    public void deleteUser(User user) {
        users.remove(user);
    }

    // 将原本使用该值的地方全部替换为新的值
    public void replaceByNewValue(Value newValue) {
        for (User user : users) {
            user.modifyOperand(this, newValue);
        }
        users.clear();
    }
}
