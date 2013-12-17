
#include <assert.h>

#include <jni.h>
#include <git2.h>

#include "repository.h"
#include "util.h"
#include "oid.h"

#define GIT_JAVA_CLASS_COMMIT "org/libgit2/jagged/Commit"
#define GIT_JAVA_CLASS_TREE "org/libgit2/jagged/Tree"

static git_object *git_java_object_native(JNIEnv *env, jobject repo_java, jobject object_java, jint type)
{
	jclass object_class;
	jmethodID object_idmethod;
	jobject oid_java;
	git_repository *repo;
	git_oid oid;
	git_object *object = NULL;
	int error = 0;

	if ((repo = git_java_handle_get(env, repo_java)) == NULL ||
		(object_class = (*env)->FindClass(env, "org/libgit2/jagged/GitObject")) == NULL ||
		(object_idmethod = (*env)->GetMethodID(env, object_class, "getId", "()Lorg/libgit2/jagged/ObjectId;")) == NULL ||
		(oid_java = (*env)->CallObjectMethod(env, object_java, object_idmethod)) == NULL ||
		git_java_objectid_tonative(&oid, env, oid_java) < 0)
		goto done;

	if ((error = git_object_lookup(&object, repo, &oid, type)) < 0)
		git_java_exception_throw_giterr(env, error);

done:
	return object;
}

GIT_INLINE(jobject) git_java_object_init(JNIEnv *env, jobject repo_java, jobject oid_java, git_object *object)
{
	jclass object_class;
	jmethodID object_initmethod;
	git_otype type;
	const char *object_classname;

	switch((type = git_object_type(object)))
	{
	case GIT_OBJ_COMMIT:
		object_classname = GIT_JAVA_CLASS_COMMIT;
		break;
	case GIT_OBJ_TREE:
		object_classname = GIT_JAVA_CLASS_TREE;
		break;
	default:
		git_java_exception_throw(env, "unknown object type: %d", type);
		return NULL;
	}

	if ((object_class = (*env)->FindClass(env, object_classname)) == NULL ||
		(object_initmethod = (*env)->GetMethodID(env, object_class, "<init>", "(Lorg/libgit2/jagged/Repository;Lorg/libgit2/jagged/ObjectId;)V")) == NULL)
		return NULL;

	return (*env)->NewObject(env, object_class, object_initmethod, repo_java, oid_java);
}

JNIEXPORT jobject JNICALL
Java_org_libgit2_jagged_core_NativeMethods_objectLookup(
	JNIEnv *env,
	jclass class,
	jobject repo_java,
	jobject oid_java,
	jint type)
{
	git_repository *repo;
	git_oid oid;
	git_object *object = NULL;
	jobject object_java = NULL;
	int error = 0;

	assert(env);
	assert(class);
	assert(repo_java);
	assert(oid_java);

	if ((repo = git_java_handle_get(env, repo_java)) == NULL ||
		git_java_objectid_tonative(&oid, env, oid_java) < 0)
		goto done;

	if ((error = git_object_lookup(&object, repo, &oid, type)) < 0) {
		git_java_exception_throw_giterr(env, error);
		goto done;
	}

	object_java = git_java_object_init(env, repo_java, oid_java, object);

done:
	git_object_free(object);
	return object_java;
}

JNIEXPORT jobject JNICALL
Java_org_libgit2_jagged_core_NativeMethods_commitGetMetadata(
	JNIEnv *env,
	jclass class,
	jobject repo_java,
	jobject commit_java)
{
	jclass signature_class, metadata_class;
	jmethodID signature_initmethod, metadata_initmethod;
	jobject committer_java, author_java, metadata_java = NULL;
	jstring committer_name_java, committer_email_java, author_name_java, author_email_java;
	git_commit *commit = NULL;
	const git_signature *committer = NULL, *author = NULL;

	GIT_UNUSED(class);

	if ((signature_class = (*env)->FindClass(env, "org/libgit2/jagged/Signature")) == NULL ||
		(signature_initmethod = (*env)->GetMethodID(env, signature_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V")) == NULL ||
		(metadata_class = (*env)->FindClass(env, "org/libgit2/jagged/core/CommitMetadata")) == NULL ||
		(metadata_initmethod = (*env)->GetMethodID(env, metadata_class, "<init>", "(Lorg/libgit2/jagged/Signature;Lorg/libgit2/jagged/Signature;)V")) == NULL ||
		(commit = (git_commit *)git_java_object_native(env, repo_java, commit_java, GIT_OBJ_COMMIT)) == NULL)
		goto done;

	committer = git_commit_committer(commit);
	author = git_commit_author(commit);

	if ((committer_name_java = git_java_utf8_to_jstring(env, committer->name)) == NULL ||
		(committer_email_java = git_java_utf8_to_jstring(env, committer->email)) == NULL ||
		(author_name_java = git_java_utf8_to_jstring(env, author->name)) == NULL ||
		(author_email_java = git_java_utf8_to_jstring(env, author->email)) == NULL ||
		(committer_java = (*env)->NewObject(env, signature_class, signature_initmethod, committer_name_java, committer_email_java)) == NULL ||
		(author_java = (*env)->NewObject(env, signature_class, signature_initmethod, author_name_java, author_email_java)) == NULL)
		goto done;

	metadata_java = (*env)->NewObject(env, metadata_class, metadata_initmethod, committer_java, author_java);

done:
	git_commit_free(commit);

	return metadata_java;
}

JNIEXPORT jobjectArray JNICALL
Java_org_libgit2_jagged_core_NativeMethods_commitGetParents(
	JNIEnv *env,
	jclass class,
	jobject repo_java,
	jobject commit_java)
{
	jclass commit_class;
	git_commit *commit = NULL, *parent = NULL;
	jobjectArray parents_java = NULL;
	jobject oid_java, parent_java;
	unsigned int parents_len, i;
	int error;

	GIT_UNUSED(class);

	if ((commit = (git_commit *)git_java_object_native(env, repo_java, commit_java, GIT_OBJ_COMMIT)) == NULL ||
		(commit_class = (*env)->FindClass(env, GIT_JAVA_CLASS_COMMIT)) == NULL)
		goto done;

	parents_len = git_commit_parentcount(commit);

	if ((parents_java = (*env)->NewObjectArray(env, parents_len, commit_class, NULL)) == NULL)
		goto done;

	for (i = 0; i < parents_len; ++i) {
		if ((error = git_commit_parent(&parent, commit, i)) < 0) {
			git_java_exception_throw_giterr(env, error);
			git_commit_free(parent);
			goto done;
		}

		if ((oid_java = git_java_objectid_init(env, git_commit_id(parent))) == NULL ||
			(parent_java = git_java_object_init(env, repo_java, oid_java, (git_object *)parent)) == NULL) {
			git_commit_free(parent);
			goto done;
		}

		(*env)->SetObjectArrayElement(env, parents_java, i, parent_java);
		git_commit_free(parent);
	}

done:
	git_commit_free(commit);

	return parents_java;
}

JNIEXPORT jobjectArray JNICALL
Java_org_libgit2_jagged_core_NativeMethods_commitGetTree(
	JNIEnv *env,
	jclass class,
	jobject repo_java,
	jobject commit_java)
{
	git_commit *commit = NULL;
	git_tree *tree = NULL;
	jclass commit_class;
	jobject oid_java, tree_java = NULL;
	int error;

	GIT_UNUSED(class);

	if ((commit = (git_commit *)git_java_object_native(env, repo_java, commit_java, GIT_OBJ_COMMIT)) == NULL ||
		(commit_class = (*env)->FindClass(env, GIT_JAVA_CLASS_COMMIT)) == NULL)
		goto done;

	if ((error = git_commit_tree(&tree, commit)) < 0) {
		git_java_exception_throw_giterr(env, error);
		goto done;
	}

	if ((oid_java = git_java_objectid_init(env, git_tree_id(tree))) == NULL ||
		(tree_java = git_java_object_init(env, repo_java, oid_java, (git_object *)tree)) == NULL)
		goto done;

done:
	git_tree_free(tree);
	git_commit_free(commit);

	return tree_java;
}

JNIEXPORT jlong JNICALL
Java_org_libgit2_jagged_core_NativeMethods_treeGetEntryCount(
	JNIEnv *env,
	jclass class,
	jobject repo_java,
	jobject tree_java)
{
	git_tree *tree = NULL;
	size_t entrycount = 0;

	GIT_UNUSED(class);

	if ((tree = (git_tree *)git_java_object_native(env, repo_java, tree_java, GIT_OBJ_TREE)) == NULL)
		goto done;

	if ((entrycount = git_tree_entrycount(tree)) > INT64_MAX)
		git_java_exception_throw(env, "Too many tree entries");

done:
	git_tree_free(tree);

	return (jlong)entrycount;
}

JNIEXPORT jobject JNICALL
Java_org_libgit2_jagged_core_NativeMethods_treeGetEntry(
	JNIEnv *env,
	jclass class,
	jobject repo_java,
	jobject tree_java,
	jlong entry_idx)
{
	jclass tree_entry_class;
	jmethodID tree_entry_initmethod;
	jobject oid_java, tree_entry_java = NULL;
	jstring name_java;
	git_tree *tree = NULL;
	const git_tree_entry *tree_entry;
	git_otype type;
	git_filemode_t mode;

	assert(entry_idx >= 0);

	GIT_UNUSED(class);

	if ((tree_entry_class = (*env)->FindClass(env, "org/libgit2/jagged/TreeEntry")) == NULL ||
		(tree_entry_initmethod = (*env)->GetMethodID(env, tree_entry_class, "<init>", "(Ljava/lang/String;Lorg/libgit2/jagged/ObjectId;II)V")) == NULL ||
		(tree = (git_tree *)git_java_object_native(env, repo_java, tree_java, GIT_OBJ_TREE)) == NULL)
		goto done;

	if ((tree_entry = git_tree_entry_byindex(tree, (size_t)entry_idx)) == NULL) {
		git_java_exception_throw(env, "Could not locate tree entry %d", entry_idx);
		goto done;
	}

	if ((name_java = git_java_utf8_to_jstring(env, git_tree_entry_name(tree_entry))) == NULL ||
		(oid_java = git_java_objectid_init(env, git_tree_entry_id(tree_entry))) == NULL)
		goto done;

	type = git_tree_entry_type(tree_entry);
	mode = git_tree_entry_filemode(tree_entry);

	tree_entry_java = (*env)->NewObject(env, tree_entry_class, tree_entry_initmethod, name_java, oid_java, type, mode);

done:
	git_tree_free(tree);

	return tree_entry_java;
}
