package utils;

import java.util.Objects;

public class Pair<K, V> {
    public K first;
    public V second;

    public Pair(K key, V value) {
        this.first = key;
        this.second = value;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        Pair<?, ?> pair = (Pair<?, ?>) o;
        return Objects.equals(first, pair.first) &&
                Objects.equals(second, pair.second);
    }

    // hashCode 方法
    @Override
    public int hashCode() {
        return Objects.hash(first, second);
    }

    // toString 方法
    @Override
    public String toString() {
        return "Pair{" +
                "key=" + first +
                ", value=" + second +
                '}';
    }

    public void setKey(K first) {
        this.first = first;
    }

    public void setValue(V value) {
        this.second = value;
    }

    public K getKey() {
        return first;
    }

    public V getValue() {
        return second;
    }
}